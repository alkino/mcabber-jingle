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
#include <jingle/send.h>


void jingle_send_session_terminate(JingleNode *jn, const gchar *reason)
{
  LmMessage *r;
  LmMessageNode *err;
  JingleNode *reply = g_new0(JingleNode, 1);

  reply->action = JINGLE_SESSION_TERMINATE;
  reply->sid = jn->sid;

  r = lm_message_from_jinglenode(reply, lm_message_get_from(jn->message));

  if (reason != NULL) { 
    // TODO check reason 
    err = lm_message_node_add_child(r->node, "reason", NULL);
    lm_message_node_add_child(err, reason, NULL);
  }

  if (r) {
    lm_connection_send(lconnection, r, NULL);
    lm_message_unref(r);
  }
}
