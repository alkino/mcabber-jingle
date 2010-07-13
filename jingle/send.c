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

#include <jingle/jingle.h>
#include <jingle/sessions.h>
#include <jingle/send.h>


void jingle_send_session_terminate(JingleNode *jn, const gchar *reason)
{
  LmMessage *r;
  LmMessageNode *err;
  JingleNode *reply = g_new0(JingleNode, 1);
  JingleAckHandle *ackhandle;

  reply->action = JINGLE_SESSION_TERMINATE;
  reply->sid = jn->sid;

  r = lm_message_from_jinglenode(reply, lm_message_get_from(jn->message));
  if (r == NULL) return;

  if (reason != NULL) { 
    // TODO check reason 
    err = lm_message_node_add_child(r->node, "reason", NULL);
    lm_message_node_add_child(err, reason, NULL);
  }

  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = NULL;
  ackhandle->user_data = NULL;

  lm_connection_send_with_reply(lconnection, r,
                                jingle_new_ack_handler(ackhandle), NULL);
  lm_message_unref(r);
}

void jingle_send_session_accept(JingleNode *jn)
{
  JingleNode accept = {0};
  JingleSession* sess;
  JingleContent *cn;
  GSList *child = NULL;
  JingleAppFuncs *appfuncs;
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  GError *err = NULL;
  JingleAckHandle *ackhandle;
 
  accept.action = JINGLE_SESSION_ACCEPT;
  accept.responder = g_strdup_printf("%s/%s",
                                     lm_connection_get_jid(lconnection),
                                     settings_opt_get("resource"));
  accept.sid = jn->sid;
  accept.content = NULL;

  sess = session_new(jn);

  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);

    xmlns = lm_message_node_get_attribute(cn->description, "xmlns");
    appfuncs = jingle_get_appfuncs(xmlns);
    if (appfuncs == NULL) continue;

    xmlns = lm_message_node_get_attribute(cn->transport, "xmlns");
    transfuncs = jingle_get_transportfuncs(xmlns);
    if (appfuncs == NULL) continue;

    description = appfuncs->check(cn, &err);
    if (description == NULL || err != NULL) continue;
    transport = transfuncs->check(cn, &err);
    if (transport == NULL || err != NULL) continue;

    session_add_content(sess, cn, JINGLE_SESSION_STATE_ACTIVE);
    accept.content = g_slist_append(accept.content, cn);
  }

  if(g_slist_length(sess->content) == 0) {
    jingle_send_session_terminate(jn, "unsupported-applications");
    session_delete(sess);
    return;
  }

  if (g_slist_length(accept.content) <= 0) return;

  accept.message = lm_message_from_jinglenode(&accept,
                                              lm_message_get_from(jn->message));
  if (accept.message) {
    ackhandle = g_new0(JingleAckHandle, 1);
    ackhandle->callback = NULL;
    ackhandle->user_data = NULL;
    lm_connection_send_with_reply(lconnection, accept.message,
                                  jingle_new_ack_handler(ackhandle), NULL);
    lm_message_unref(accept.message);
  }
}
