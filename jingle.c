/*
 *  jingle.c -- Base jingle functions
 *
 * Copyrigth (C) 2010    Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
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

#include <glib.h>
#include <loudmouth/loudmouth.h>
#include <string.h>

#include <mcabber/xmpp.h>
#include <mcabber/hooks.h>
#include <mcabber/modules.h>
#include <mcabber/logprint.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/xmpp_defines.h>

#include "jingle.h"
#include "jingle_register.h"
#include "parse.h"

static void  jingle_register_lm_handlers(void);
static void  jingle_unregister_lm_handlers(void);
static guint jingle_connect_hh(const gchar *hname, hk_arg_t *args, gpointer ignore);
static guint jingle_disconn_hh(const gchar *hname, hk_arg_t *args, gpointer ignore);
static void  jingle_init(void);
static void  jingle_uninit(void);


static LmMessageHandler* jingle_iq_handler = NULL;
static guint connect_hid = 0;
static guint disconn_hid = 0;


module_info_t info_jingle = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = MCABBER_VERSION,
  .description     = "Main Jingle module,"
    " required for file transport, voip...\n",
  .requires        = NULL,
  .init            = jingle_init,
  .uninit          = jingle_uninit,
  .next            = NULL,
};


LmHandlerResult jingle_handle_iq(LmMessageHandler *handler,
    LmConnection *connection,
    LmMessage *message,
    gpointer user_data)
{
  LmMessageSubType iqtype = lm_message_get_sub_type(message);
  if (iqtype != LM_MESSAGE_SUB_TYPE_SET)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  struct jingle_data ij;
  LmMessageNode *root = lm_message_get_node(message)->children;
  LmMessageNode *node = lm_message_node_get_child(root, "jingle");

  if (!node) // no <jingle> element found
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  if (g_strcmp0(lm_message_node_get_attribute(node, "xmlns"), NS_JINGLE)) {
    scr_log_print(LPRINT_DEBUG, "jingle: Received a jingle IQ with an invalid namespace");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  //parse_iq(lm_message_get_node(message), &ii);

  // On traitre les erreurs
  if (parse_jingle(node, &ij) != PARSE_OK) {
   
   
   
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  scr_log_print(LPRINT_DEBUG, "jingle: Received a jingle IQ");

  jingle_error_iq(message, "cancel", "feature-not-implemented", "unsupported-info");

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
 * Reply to a Jingle IQ with an error.
 */
void jingle_error_iq(LmMessage *m, const gchar *errtype,
    const gchar *cond, const gchar *jinglecond)
{
  LmMessage *r;
  LmMessageNode *err, *tmpnode;

  r = lm_message_new_iq_from_query(m, LM_MESSAGE_SUB_TYPE_ERROR);
  err = lm_message_node_add_child(r->node, "error", NULL);
  lm_message_node_set_attribute(err, "type", errtype);

  // error condition as defined by RFC 3920bis section 8.3.3
  tmpnode = lm_message_node_add_child(err, cond, NULL);
  lm_message_node_set_attribute(tmpnode, "xmlns", NS_XMPP_STANZAS);

  // jingle error condition as defined by XEP-0166 section 10
  tmpnode = lm_message_node_add_child(err, jinglecond, NULL);
  lm_message_node_set_attribute(tmpnode, "xmlns", NS_JINGLE_ERRORS);

  lm_connection_send(lconnection, r, NULL);
  lm_message_unref(r);
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
