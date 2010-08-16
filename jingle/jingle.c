/*
 * jingle.c
 *
 * Copyrigth (C) 2010 Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include <mcabber/xmpp.h>
#include <mcabber/hooks.h>
#include <mcabber/modules.h>
#include <mcabber/logprint.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/events.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/action-handlers.h>
#include <jingle/register.h>
#include <jingle/send.h>
#include <jingle/sessions.h>

static void  jingle_register_lm_handlers(void);
static void  jingle_unregister_lm_handlers(void);
static guint jingle_connect_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore);
static guint jingle_disconn_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore);
static void  jingle_init(void);
static void  jingle_uninit(void);

static LmMessageHandler* jingle_iq_handler = NULL;
static GSList *ack_handlers = NULL;
static guint ack_timeout_checker = 0;
static guint connect_hid = 0;
static guint disconn_hid = 0;


/**
 * Must be aligned with the values in JingleAction
 * for easy acces.
 */
struct JingleActionList jingle_action_list[] = {
  { NULL,                NULL }, // for JINGLE_UNKNOWN_ACTION
  { "content-accept",    NULL },
  { "content-add",       NULL },
  { "content-modify",    NULL },
  { "content-reject",    NULL },
  { "content-remove",    NULL },
  { "description-info",  NULL },
  { "security-info",     NULL },
  { "session-accept",    handle_session_accept },
  { "session-info",      handle_session_info },
  { "session-initiate",  handle_session_initiate },
  { "session-terminate", handle_session_terminate },
  { "transport-accept",  NULL },
  { "transport-info",    NULL },
  { "transport-reject",  NULL },
  { "transport-replace", NULL },
};

module_info_t info_jingle = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Main Jingle module,"
                     " required for file transport, voip...\n",
  .requires        = NULL,
  .init            = jingle_init,
  .uninit          = jingle_uninit,
  .next            = NULL,
};


LmHandlerResult jingle_handle_iq(LmMessageHandler *handler,
                                 LmConnection *connection, LmMessage *message,
                                 gpointer user_data)
{
  LmMessageSubType iqtype = lm_message_get_sub_type(message);
  if (iqtype != LM_MESSAGE_SUB_TYPE_SET)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  JingleNode *jn = g_new0(JingleNode, 1);
  GError *error = NULL;
  LmMessageNode *root = lm_message_get_node(message);
  LmMessageNode *jnode = lm_message_node_get_child(root, "jingle");

  if (!jnode) // no <jingle> element found
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  if (g_strcmp0(lm_message_node_get_attribute(jnode, "xmlns"), NS_JINGLE)) {
    scr_log_print(LPRINT_DEBUG,
                  "jingle: Received a jingle IQ with an invalid namespace");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  check_jingle(message, jnode, jn, &error);
  if (error != NULL) {
    if (error->domain == JINGLE_CHECK_ERROR) {
      // request malformed, we reply with a bad-request
      jingle_send_iq_error(message, "cancel", "bad-request", NULL);
      scr_log_print(LPRINT_DEBUG,
                    "jingle: %s", error->message);
    }
    g_error_free(error);
    jingle_free_jinglenode(jn);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  scr_log_print(LPRINT_DEBUG, "jingle: Received a valid jingle IQ");

  if (jingle_action_list[jn->action].handler == NULL) {
    jingle_send_iq_error(message, "cancel", "feature-not-implemented", NULL);
    jingle_free_jinglenode(jn);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  jingle_action_list[jn->action].handler(jn);
  jingle_free_jinglenode(jn);
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

/**
 * Handle incoming ack iq (type result or error).
 */
LmHandlerResult jingle_handle_ack_iq(LmMessageHandler *handler,
                                     LmConnection *connection, 
                                     LmMessage *message, gpointer user_data)
{
  // TODO: check subtype
  if (user_data != NULL) {
    JingleAckHandle *ah = user_data;
    if(ah->callback != NULL)
      ah->callback(JINGLE_ACK_RESPONSE, message, ah->user_data);

    jingle_ack_handler_free(ah);
  }

  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean jingle_ack_timeout_checker(gpointer user_data)
{
  GSList *el, *prev;
  time_t now = time(NULL);

  el = prev = ack_handlers;
  while(el) {
    JingleAckHandle *ah = el->data;

    if (ah->timeout != 0 && ah->_inserted + ah->timeout <= now) {
      if (ah->callback != NULL) {
        ah->callback(JINGLE_ACK_TIMEOUT, NULL, ah->user_data);
      }

      jingle_ack_handler_free(ah);
      if (ack_handlers == NULL)
        break;

      el = prev;
    }
    prev = el;
    el = el->next;
  }
  return TRUE;
}

LmMessageHandler *jingle_new_ack_handler(JingleAckHandle *ah)
{
  if(ack_timeout_checker == 0)
	  ack_timeout_checker = g_timeout_add_seconds(3, jingle_ack_timeout_checker, NULL);
  
  ah->_inserted = time(NULL);
  ah->_handler = lm_message_handler_new(jingle_handle_ack_iq,
                                        (gpointer) ah, NULL);
  ack_handlers = g_slist_append(ack_handlers, ah);
  return ah->_handler;
}

void jingle_ack_handler_free(JingleAckHandle *ah)
{
  lm_message_handler_unref(ah->_handler);
  ack_handlers = g_slist_remove(ack_handlers, ah);
  g_free(ah);
}

/**
 * mcabber /event callback.
 */
gboolean evscallback_jingle(guint evcontext, const gchar *arg,
                            gpointer userdata)
{
  JingleSession *js = (JingleSession*)userdata;

  /*
  if (G_UNLIKELY(!jn)) {
    scr_LogPrint(LPRINT_LOGNORM, "Error in evs callback.");
    return FALSE;
  }
  */

  if (evcontext == EVS_CONTEXT_TIMEOUT) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s timed out, cancelled.",
                 js->from);
    return FALSE;
  }
  if (evcontext == EVS_CONTEXT_CANCEL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s cancelled.",
                 js->from);
    return FALSE;
  }
  if (!(evcontext == EVS_CONTEXT_ACCEPT || evcontext == EVS_CONTEXT_REJECT)) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s cancelled.",
                 js->from);
    return FALSE;
  }
  
  if (evcontext == EVS_CONTEXT_ACCEPT) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s accepted.",
                 js->from);
    jingle_send_session_accept(js);
  } else {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s cancelled.",
                 js->from);
    jingle_send_session_terminate(js, "decline");
    
  }

  return FALSE;
}

/**
 * According to the specifications:
 * "An entity that receives an IQ request of type "get" or "set" MUST
 * reply with an IQ response of type "result" or "error"."
 * For Jingle's IQ, we have to reply with an empty "result" IQ to acknowledge
 * receipt.
 */
void jingle_ack_iq(LmMessage *m)
{
  LmMessage *r;

  r = lm_message_new_iq_from_query(m, LM_MESSAGE_SUB_TYPE_RESULT);
  lm_connection_send(lconnection, r, NULL);
  lm_message_unref(r);
}

/**
 * Create an error IQ.
 */
LmMessage *jingle_new_iq_error(LmMessage *m, const gchar *errtype,
                               const gchar *cond, const gchar *jinglecond)
{
  LmMessage *r;
  LmMessageNode *err, *tmpnode;

  r = lm_message_new_iq_from_query(m, LM_MESSAGE_SUB_TYPE_ERROR);
  err = lm_message_node_add_child(r->node, "error", NULL);
  lm_message_node_set_attribute(err, "type", errtype);

  // error condition as defined by RFC 3920bis section 8.3.3
  if (cond != NULL) {
    tmpnode = lm_message_node_add_child(err, cond, NULL);
    lm_message_node_set_attribute(tmpnode, "xmlns", NS_XMPP_STANZAS);
  }

  // jingle error condition as defined by XEP-0166 section 10
  if (jinglecond != NULL) {
    tmpnode = lm_message_node_add_child(err, jinglecond, NULL);
    lm_message_node_set_attribute(tmpnode, "xmlns", NS_JINGLE_ERRORS);
  }

  return r;
}

/**
 * Reply to a Jingle IQ with an error.
 */
void jingle_send_iq_error(LmMessage *m, const gchar *errtype,
                          const gchar *cond, const gchar *jinglecond)
{
  LmMessage *r = jingle_new_iq_error(m, errtype, cond, jinglecond);
  if (r) {
    lm_connection_send(lconnection, r, NULL);
  }
}

/**
 * Find the best resource to initiate a jingle session.
 * Test every ressource for a given jid and return the one
 * who support all namespaces in ns.
 */
gchar *jingle_find_compatible_res(const gchar *jid, const gchar *ns[])
{
  gchar *choosenres;
  guint indexns;
  gboolean found;
  GList *roster_usr;
  GSList *reslist, *thisres;

  roster_usr = buddy_search_jid(jid);
  reslist = buddy_getresources(roster_usr->data);
  for (thisres = reslist; thisres; thisres = g_slist_next(thisres)) {
    found = TRUE;
    for (indexns = 0; ns[indexns]; indexns++) {
	  // if (!caps_has_feature(buddy_resource_getcaps(roster_usr->data, thisres->data), ns[indexns]))
	   //  found = FALSE;
	 }
	 if (!found) continue;

    choosenres = g_strdup(thisres->data);
    g_slist_foreach(reslist, (GFunc)g_free, NULL);
    g_slist_free(reslist);
    return choosenres;
  }

  return NULL;
}

/**
 * Find the jingle_action corresponding to a string
 */
JingleAction jingle_action_from_str(const gchar *string)
{
  guint i, actstrlen = sizeof(jingle_action_list)/sizeof(jingle_action_list[0]);
  for (i = 0; i < actstrlen; i++)
    if (!g_strcmp0(jingle_action_list[i].name, string))
      return (JingleAction) i;

  return JINGLE_UNKNOWN_ACTION;
}

/**
 * This function should not be called if jn->message was created
 * using lm_message_from_jinglenode because loudmouth strdup
 * strings we insert as attributes. the pointers in the LmMessage
 * and the JingleNode would not refer to the same addresses and
 * a call to lm_message_unref would not free the data from the
 * JingleNode.
 */
void jingle_free_jinglenode(JingleNode *jn)
{
  g_slist_foreach(jn->content, (GFunc)g_free, NULL);
  g_slist_free(jn->content);
  lm_message_unref(jn->message);
  g_free(jn);
}

static void jingle_unregister_lm_handlers(void)
{
  if (lconnection) {
    lm_connection_unregister_message_handler(lconnection, jingle_iq_handler,
        LM_MESSAGE_TYPE_IQ);
  }
}

static void jingle_register_lm_handlers(void)
{
  jingle_unregister_lm_handlers();
  if (lconnection) {
    lm_connection_register_message_handler(lconnection, jingle_iq_handler,
        LM_MESSAGE_TYPE_IQ,
        LM_HANDLER_PRIORITY_FIRST);
  }
}

static guint jingle_connect_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore)
{
  jingle_register_lm_handlers();
  return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static guint jingle_disconn_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore)
{
  jingle_unregister_lm_handlers();
  return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void jingle_init(void)
{
  jingle_iq_handler = lm_message_handler_new(jingle_handle_iq, NULL, NULL);
  xmpp_add_feature(NS_JINGLE);

  connect_hid = hk_add_handler(jingle_connect_hh, HOOK_POST_CONNECT,
      G_PRIORITY_DEFAULT_IDLE, NULL);
  disconn_hid = hk_add_handler(jingle_disconn_hh, HOOK_PRE_DISCONNECT,
      G_PRIORITY_DEFAULT_IDLE, NULL);
  jingle_register_lm_handlers();
}

static void jingle_uninit(void)
{
  xmpp_del_feature(NS_JINGLE);

  hk_del_handler(HOOK_POST_CONNECT, connect_hid);
  hk_del_handler(HOOK_PRE_DISCONNECT, disconn_hid);
  jingle_unregister_lm_handlers();

  lm_message_handler_invalidate(jingle_iq_handler);
  lm_message_handler_unref(jingle_iq_handler);

  if (ack_timeout_checker != 0) {
    GSource *s = g_main_context_find_source_by_id(NULL, ack_timeout_checker);
    g_source_destroy(s);
  }
}

void handle_trans_data(gconstpointer data, const gchar *data2, guint len)
{
  SessionContent *sc = session_find_transport(data);
  if (sc == NULL) {
    return;  
  }
  sc->appfuncs->handle_data(sc->description, data2, len);
}

void handle_trans_next(session_content *sc2) {
  JingleSession *sess = session_find_by_sid(sc2->sid, sc2->from);
  if (sess == NULL) {
    // TODO: err
    return;
  }
  
  SessionContent *sc = session_find_sessioncontent(sess, sc2->name);
  
  // TODO: size!
  sc->appfuncs->send(sc2);
  g_free(sc2);
}

gchar *jingle_generate_sid(void)
{
  gchar *sid;
  gchar car[] = "azertyuiopqsdfghjklmwxcvbn1234567890AZERTYUIOPQSDFGHJKLMWXCVBN";
  int i;
  sid = g_new0(gchar, 11);
  for (i = 0; i < 10; i++)
    sid[i] = car[g_random_int_range(0, sizeof(car)/sizeof(car[0]))];

  sid[10] = '\0';
  
  return sid;
}
