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
#include "parse.h"


static void jingle_init  (void);
static void jingle_uninit(void);


LmMessageHandler* jingle_iq_handler = NULL;


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
  //struct iq_data ii;
  struct jingle_data ij;
  LmMessageNode *root = lm_message_get_node(message)->children;
  LmMessageNode *node = lm_message_node_get_child(root, "jingle");

  if (!node)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; // no <jingle> element found
  
  if (g_strcmp0(lm_message_node_get_attribute(node, "xmlns"), NS_JINGLE)) {
    scr_log_print(LPRINT_DEBUG, "jingle: Received a jingle IQ with an invalid namespace");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  //parse_iq(lm_message_get_node(message), &ii);

  if (parse_jingle(node, &ij) != PARSE_OK)
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    
  scr_log_print(LPRINT_DEBUG, "jingle: Received a jingle IQ");

  jingle_error_iq(message, "cancel", "feature-not-implemented", "unsupported-info");

  /*if (!strcmp(ij.action, "content-accept")) {
  } else if (!strcmp(ij.action, "content-add")) {
  } else if (!strcmp(ij.action, "content-modify")) {
  } else if (!strcmp(ij.action, "content-reject")) {
  } else if (!strcmp(ij.action, "content-remove")) {
  } else if (!strcmp(ij.action, "description-info")) {
  } else if (!strcmp(ij.action, "security-info")) {
  } else if (!strcmp(ij.action, "session-accept")) {
  } else if (!strcmp(ij.action, "session-info")) {
  } else if (!strcmp(ij.action, "session-initiate")) {
  } else if (!strcmp(ij.action, "session-terminate")) {
  } else if (!strcmp(ij.action, "transport-accept")) {
  } else if (!strcmp(ij.action, "transport-info")) {
  } else if (!strcmp(ij.action, "transport-reject")) {
  } else if (!strcmp(ij.action, "transport-replace")) {
  }*/

  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

/**
 * According to the specifications:
 * "An entity that receives an IQ request of type "get" or "set" MUST
 * reply with an IQ response of type "result" or "error"."
 * For Jingle's IQ, we have to reply with an empty "result" IQ to acknowledge
 * receipt.
 */
void jingle_ack_iq(LmMessage *m) {
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

static void jingle_init(void)
{
  jingle_iq_handler = lm_message_handler_new(jingle_handle_iq, NULL, NULL);
  lm_connection_register_message_handler(lconnection, jingle_iq_handler, LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_FIRST);
  xmpp_add_feature(NS_JINGLE);
  xmpp_add_feature("urn:xmpp:jingle:apps:file-transfer:info:1"); // for debugging puposes only...
}

static void jingle_uninit(void)
{
  lm_connection_unregister_message_handler(lconnection, jingle_iq_handler, LM_MESSAGE_TYPE_IQ);
  lm_message_handler_invalidate(jingle_iq_handler);
  lm_message_handler_unref(jingle_iq_handler);
}
