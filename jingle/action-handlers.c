/*
 * action-handlers.c
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

#include <jingle/jingle.h>
#include <jingle/check.h>


void handle_session_initiate(LmMessage *m, JingleNode *jn)
{
  // a session-initiate message must contains at least one <content> element
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(m, "cancel", "bad-request", NULL);
    return;
  }

  /*// if a session with the same sid already exists
  if (session_find(jn) != NULL) {
    jingle_send_iq_error(m, "cancel", "unexpected-request", "out-of-order");
    return;
  }*/

  jingle_ack_iq(m);
}

void handle_session_terminate(LmMessage *m, JingleNode *jn)
{
  /*if (session_find(jn) == NULL) {
    jingle_send_iq_error(m, "cancel", "item-not-found", "unknown-session");
    return;
  }*/
}
