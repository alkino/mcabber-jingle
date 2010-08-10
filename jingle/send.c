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


void jingle_send_session_terminate(JingleNode *jn, const gchar *reason)
{
  LmMessage *r;
  LmMessageNode *err,*err2;
  JingleNode *reply = g_new0(JingleNode, 1);
  JingleAckHandle *ackhandle;

  reply->action = JINGLE_SESSION_TERMINATE;
  reply->sid = jn->sid;

  r = lm_message_from_jinglenode(reply, lm_message_get_from(jn->message));
  if (r == NULL) return;

  if (reason != NULL) { 
    err2 = lm_message_node_get_child(r->node, "jingle");
    // TODO check reason 
    err = lm_message_node_add_child(err2, "reason", NULL);
    lm_message_node_add_child(err, reason, NULL);
  }

  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = NULL;
  ackhandle->user_data = NULL;

  lm_connection_send_with_reply(lconnection, r,
                                jingle_new_ack_handler(ackhandle), NULL);
  lm_message_unref(r);
}

static void jingle_handle_ack_iq_sa(LmMessage *mess, gpointer *data)
{
  LmMessageNode *node;
  const gchar *type, *cause;
  JingleSession *sess = (JingleSession*)data;
  
  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_RESULT)
    return;
  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_ERROR) {
    node = lm_message_get_node(mess);
    node = lm_message_node_get_child(node,"error");
    type = lm_message_node_get_attribute(node, "type");
    if(node->children != NULL)
      cause = node->children->name;
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: session-accept %s: %s", type, cause);
  }
  // We delete the session, there was an error
  session_delete(sess);
}

void jingle_send_session_accept(JingleNode *jn)
{
  JingleSession *sess;
  JingleContent *cn;
  LmMessage *mess;
  GSList *child = NULL;
  JingleAppFuncs *appfuncs;
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  GError *err = NULL;
  JingleAckHandle *ackhandle;
 
  sess = session_new_from_jinglenode(jn);

  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);

    xmlns = lm_message_node_get_attribute(cn->description, "xmlns");
    appfuncs = jingle_get_appfuncs(xmlns);
    if (appfuncs == NULL) continue;

    xmlns = lm_message_node_get_attribute(cn->transport, "xmlns");
    transfuncs = jingle_get_transportfuncs(xmlns);
    if (transfuncs == NULL) continue;

    description = appfuncs->check(cn, &err);
    if (description == NULL || err != NULL) continue;
    transport = transfuncs->check(cn, &err);
    if (transport == NULL || err != NULL) continue;
   
    scr_log_print(LPRINT_DEBUG, "jingle: New content accepted: %s", cn->name);

    session_add_content_from_jinglecontent(sess, cn, JINGLE_SESSION_STATE_ACTIVE);
  }

  if(g_slist_length(sess->content) == 0) {
    jingle_send_session_terminate(jn, "unsupported-applications");
    session_delete(sess);
    return;
  }

  mess = lm_message_from_jinglesession(sess, JINGLE_CONTENT_ACCEPT);
  if (mess) {
    ackhandle = g_new0(JingleAckHandle, 1);
    ackhandle->callback = jingle_handle_ack_iq_sa;
    ackhandle->user_data = (gpointer*)sess;
    lm_connection_send_with_reply(lconnection, mess,
                                  jingle_new_ack_handler(ackhandle), NULL);
    lm_message_unref(mess);
  }
}

static void jingle_handle_ack_iq_si(LmMessage *mess, gpointer *data)
{
  LmMessageNode *node;
  const gchar *type, *cause;
  JingleSession *sess = (JingleSession*)data;
  
  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_RESULT)
    return;
  if(lm_message_get_sub_type(mess) == LM_MESSAGE_SUB_TYPE_ERROR) {
    node = lm_message_get_node(mess);
    node = lm_message_node_get_child(node,"error");
    type = lm_message_node_get_attribute(node, "type");
    if(node->children != NULL)
      cause = node->children->name;
    scr_LogPrint(LPRINT_LOGNORM, "Jingle: session-initiate %s: %s", type, cause);
  }
  // We delete the session, there was an error
  session_delete(sess);
}

void jingle_send_session_initiate(JingleSession *js)
{
  JingleAckHandle *ackhandle;
  GSList *listentry;
  
  LmMessage *mess = lm_message_from_jinglesession(js, JINGLE_SESSION_INITIATE);
  lm_message_node_set_attribute(lm_message_node_get_child(mess->node, "jingle"),
                                "initiator", js->from);

  if (mess) {
    ackhandle = g_new0(JingleAckHandle, 1);
    ackhandle->callback = jingle_handle_ack_iq_si;
    ackhandle->user_data = (gpointer*)js;
    lm_connection_send_with_reply(lconnection, mess,
                                  jingle_new_ack_handler(ackhandle), NULL);
    lm_message_unref(mess);
  }
}
