/*
 * send.c
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

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include <mcabber/xmpp_helper.h>
#include <mcabber/xmpp_defines.h>
#include <mcabber/settings.h>
#include <mcabber/logprint.h>

#include <jingle/jingle.h>
#include <jingle/sessions.h>
#include <jingle/send.h>


void jingle_send_session_terminate(JingleSession *js, const gchar *reason)
{
  JingleAckHandle *ackhandle;
  LmMessageNode *node2;
  LmMessage *r = lm_message_new_with_sub_type(js->to, LM_MESSAGE_TYPE_IQ,
                                              LM_MESSAGE_SUB_TYPE_SET);
  LmMessageNode *node = lm_message_get_node(r);
  node2 = lm_message_node_add_child(node, "jingle", NULL);
  lm_message_node_set_attributes(node2, "xmlns", NS_JINGLE,
                                 "action", "session-terminate",
                                 "sid", js->sid,
                                 NULL);
  if (r == NULL) return;

  if (reason != NULL) { 
    node = lm_message_node_add_child(node2, "reason", NULL);
    lm_message_node_add_child(node, reason, NULL);
  }

  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = NULL;
  ackhandle->user_data = NULL;

  lm_connection_send_with_reply(lconnection, r,
                                jingle_new_ack_handler(ackhandle), NULL);
  lm_message_unref(r);
}

static void jingle_handle_ack_iq_sa(JingleAckType acktype, LmMessage *mess,
                                    gpointer data)
{
  LmMessageNode *node;
  const gchar *type, *cause;
  JingleSession *sess = (JingleSession*)data;

  if (acktype == JINGLE_ACK_TIMEOUT) {
    // TODO: handle ack timeout...
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: did not receive the accept ack in time, aborting");
    session_delete(sess);
    return;
  }

  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_RESULT) {
    return;
  }

  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_ERROR) {
    node = lm_message_get_node(mess);
    node = lm_message_node_get_child(node,"error");
    type = lm_message_node_get_attribute(node, "type");
    if(node->children != NULL)
      cause = node->children->name;
    else
      cause = "?";
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: session-accept %s: %s", type, cause);
  }
  // We delete the session, there was an error
  session_delete(sess);
}

void jingle_send_session_accept(JingleSession *js)
{
  LmMessage *mess;
  JingleAckHandle *ackhandle;



  mess = lm_message_from_jinglesession(js, JINGLE_SESSION_ACCEPT);
 
  if (mess) {
    ackhandle = g_new0(JingleAckHandle, 1);
    ackhandle->callback = jingle_handle_ack_iq_sa;
    ackhandle->user_data = (gpointer)js;
    ackhandle->timeout = 60;
    lm_connection_send_with_reply(lconnection, mess,
                                  jingle_new_ack_handler(ackhandle), NULL);
    lm_message_unref(mess);
  }
}

static void jingle_handle_ack_iq_si(JingleAckType acktype, LmMessage *mess,
                                    gpointer data)
{
  LmMessageNode *node;
  const gchar *type, *cause;
  JingleSession *sess = (JingleSession*)data;

  if (acktype == JINGLE_ACK_TIMEOUT) {
    // TODO: handle ack timeout...
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: did not receive the initiate ack in time, aborting");
    session_delete(sess);
    return;
  }

  if (lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_RESULT)
    return;
  if (lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_ERROR) {
    node = lm_message_get_node(mess);
    node = lm_message_node_get_child(node,"error");
    type = lm_message_node_get_attribute(node, "type");
    if(node->children != NULL)
      cause = node->children->name;
    else
      cause = "?";
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: session-initiate %s: %s", type, cause);
  }
  // We delete the session, there was an error
  session_delete(sess);
}

void jingle_send_session_initiate(JingleSession *js)
{
  JingleAckHandle *ackhandle;
  GError *err = NULL;
  gboolean status;
  
  LmMessage *mess = lm_message_from_jinglesession(js, JINGLE_SESSION_INITIATE);
  lm_message_node_set_attribute(lm_message_node_get_child(mess->node, "jingle"),
                                "initiator", js->from);
 
  if (mess) {
    ackhandle = g_new0(JingleAckHandle, 1);
    ackhandle->callback = jingle_handle_ack_iq_si;
    ackhandle->user_data = (gpointer)js;
    ackhandle->timeout = 60;
    status = lm_connection_send_with_reply(lconnection, mess,
                                       jingle_new_ack_handler(ackhandle), &err);
    // TODO: delete the ack_handler
    if (status == FALSE || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle: %s: try again", err->message);
      session_delete(js);
    }         
    lm_message_unref(mess);
  }
}
