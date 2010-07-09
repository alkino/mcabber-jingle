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
#include <mcabber/xmpp_defines.h> 
#include <mcabber/events.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/action-handlers.h>
#include <jingle/register.h>


static void  jingle_register_lm_handlers(void);
static void  jingle_unregister_lm_handlers(void);
static guint jingle_connect_hh(const gchar *hname, hk_arg_t *args, gpointer ignore);
static guint jingle_disconn_hh(const gchar *hname, hk_arg_t *args, gpointer ignore);
static void  jingle_init(void);
static void  jingle_uninit(void);
static void lm_insert_jinglecontent(gpointer data, gpointer userdata);


static LmMessageHandler* jingle_iq_handler = NULL;
static guint connect_hid = 0;
static guint disconn_hid = 0;

/**
 * Must be aligned with the values in JingleAction
 * for easy acces.
 */
struct JingleActionList jingle_action_list[] = {
  { NULL,                NULL }, // for JINGLE_UNKNOWN_ACTION
  { "content-accept",    handle_content_accept },
  { "content-add",       handle_content_add },
  { "content-modify",    NULL },
  { "content-reject",    handle_content_reject },
  { "content-remove",    handle_content_remove },
  { "description-info",  NULL },
  { "security-info",     NULL },
  { "session-accept",    NULL },
  { "session-info",      NULL },
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
    scr_log_print(LPRINT_DEBUG, "jingle: Received a jingle IQ with an invalid namespace");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  check_jingle(message, jnode, jn, &error);
  if (error != NULL) {
    if (error->domain == JINGLE_CHECK_ERROR) {
      // request malformed, we reply with a bad-request
      jingle_send_iq_error(message, "cancel", "bad-request", NULL);
    }
    g_clear_error(&error);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  scr_log_print(LPRINT_DEBUG, "jingle: Received a valid jingle IQ");

  if (jingle_action_list[jn->action].handler == NULL) {
    jingle_send_iq_error(message, "cancel", "feature-not-implemented", NULL);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  jingle_action_list[jn->action].handler(jn);

  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
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
	  lm_message_unref(r);
  }
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

static guint jingle_connect_hh(const gchar *hname, hk_arg_t *args, gpointer ignore)
{
  jingle_register_lm_handlers();
  return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static guint jingle_disconn_hh(const gchar *hname, hk_arg_t *args, gpointer ignore)
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
}

LmMessage *lm_message_from_jinglenode(const JingleNode *jn, const gchar *to)
{
  LmMessage* m; 
  LmMessageNode *jnode;
  const gchar *actionstr;

  m = lm_message_new_with_sub_type(to, LM_MESSAGE_TYPE_IQ,
                                   LM_MESSAGE_SUB_TYPE_SET); 
  jnode = lm_message_node_add_child(m->node, "jingle", NULL);

  if ((actionstr = jingle_action_list[jn->action].name))
    lm_message_node_set_attribute(jnode, "action", actionstr);
  else 
    return NULL;

  if (jn->initiator)
    lm_message_node_set_attribute(jnode, "initiator", jn->initiator);

  if (jn->responder)
    lm_message_node_set_attribute(jnode, "responder", jn->responder);

  if (jn->sid)
    lm_message_node_set_attribute(jnode, "sid", jn->sid);
  else
    return NULL;

  g_slist_foreach(jn->content, lm_insert_jinglecontent, jnode);
  return m;
}

static void lm_insert_jinglecontent(gpointer data, gpointer userdata)
{
  JingleContent* content = (JingleContent*) data;
  LmMessageNode* dad = (LmMessageNode*) userdata;
  LmMessageNode* node = (LmMessageNode*) lm_message_node_add_child(dad, "content", NULL);
  
  if (content->creator == JINGLE_CREATOR_INITIATOR)
    lm_message_node_set_attribute(node, "creator", "initiator");
  else
    lm_message_node_set_attribute(node, "creator", "responder");
  
  if (content->disposition)
    lm_message_node_set_attribute(node, "disposition", content->disposition);
  
  if (content->name)
    lm_message_node_set_attribute(node, "name", content->name);
  
  if (content->senders == JINGLE_SENDERS_BOTH)
    lm_message_node_set_attribute(node, "senders", "both");
  else if (content->senders == JINGLE_SENDERS_INITIATOR)
    lm_message_node_set_attribute(node, "senders", "initiator");
  else if (content->senders == JINGLE_SENDERS_RESPONDER)
    lm_message_node_set_attribute(node, "senders", "responder");
}

gboolean evscallback_jingle(guint evcontext, const gchar *arg, gpointer userdata)
{
  JingleNode *jn = userdata;

  // Demande à mcKael l'utilité de ce truc
  /*
  if (G_UNLIKELY(!jn)) {
    scr_LogPrint(LPRINT_LOGNORM, "Error in evs callback.");
    return FALSE;
  }
  */

  if (evcontext == EVS_CONTEXT_TIMEOUT) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s timed out, cancelled.", jn->initiator);
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  if (evcontext = EVS_CONTEXT_CANCEL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle event from %s cancelled.", jn->initiator);
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  if (!(evcontext == EVS_CONTEXT_ACCEPT || evcontext == EVS_CONTEXT_REJECT)) {
    jingle_free_jinglenode(jn);
    return FALSE;
  }
  
  if (evcontext == EVS_CONTEXT_ACCEPT) {
    jingle_send_session_accept(jn);
  } else {
    // TODO: detroy the JingleNode and send a session-reject
    jingle_free_jinglenode(jn);
  }

  return FALSE;
}
