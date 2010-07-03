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

#include <mcabber/logprint.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/sessions.h>
#include <jingle/register.h>
#include <jingle/send.h>

/* The session-initiate action is used to request negotiation of a new Jingle
 * session. When sending a session-initiate with one <content/> element, the
 * value of the <content/> element's 'disposition' attribute MUST be "session"
 * (if there are multiple <content/> elements then at least one MUST have a
 * disposition of "session"); if this rule is violated, the responder MUST
 * return a <bad-request/> error to the initiator.
 */

void handle_session_initiate(LmMessage *m, JingleNode *jn)
{
  GError *err = NULL;
  gboolean is_session = FALSE;
  GSList *child = NULL;
  JingleContent *cn;

  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(m, "cancel", "bad-request", NULL);
    return;
  }

  // a session-initiate message must contains at least one <content> element
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(m, "cancel", "bad-request", NULL);
    return;
  }
  
  // one of the content element must be a "session"
  for (child = jn->content; child && !is_session; child = child->next) {
    if(g_strcmp0(((JingleContent*)(child->data))->disposition, "session") ||
       ((JingleContent*)(child->data))->disposition == NULL) // default: session
      is_session=TRUE;
  }
  if(!is_session) {
    jingle_send_iq_error(m, "cancel", "bad-request", NULL);
    return;  
  }
  
  // if a session with the same sid already exists
  if (session_find(jn->sid, jn->from) != NULL) {
    jingle_send_iq_error(m, "cancel", "unexpected-request", "out-of-order");
    return;
  }

  // the important from is one in the session-initiate
  jn->from = lm_message_node_get_attribute(lm_message_get_node(m), "from");
  
  
  jingle_ack_iq(m);

  is_session = FALSE;  
  // Do we support any of this xmlns ?
  for (child = jn->content; child && !is_session; child = child->next) {
    if(jingle_get_appfuncs(((JingleContent*)(child->data))->xmlns_desc) != NULL)
      is_session = TRUE;
  }
  if(!is_session) { // None of the app is supported
    jingle_send_session_terminate(m, "unsupported-applications");
  }
}

void handle_session_terminate(LmMessage *m, JingleNode *jn)
{
  JingleSession *sess;
  if ((sess = session_find(jn->sid, jn->from)) == NULL) {
    jingle_send_iq_error(m, "cancel", "item-not-found", "unknown-session");
    return;
  }
  session_delete(sess);
}
