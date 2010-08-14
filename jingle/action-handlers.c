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
#include <mcabber/xmpp_helper.h>
#include <mcabber/events.h>
#include <mcabber/hbuf.h>
#include <mcabber/utils.h>
#include <mcabber/screen.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/sessions.h>
#include <jingle/register.h>
#include <jingle/send.h>
#include <jingle/action-handlers.h>


extern LmMessageHandler* jingle_ack_iq_handler;

void handle_session_initiate(JingleNode *jn)
{
  GError *err = NULL;
  gboolean valid_disposition = FALSE;
  JingleContent *cn;
  GString *sbuf;
  GSList *child = NULL;
  LmMessage *r;
  gchar *disp;
  JingleSession *sess;
  const gchar *xmlns;
  JingleAppFuncs *appfuncs;
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  
  // Make sure the request come from an user in our roster
  disp = jidtodisp(lm_message_get_from(jn->message));
  if (!roster_find(disp, jidsearch, 0)) {
    // We say that we doesn't support jingle.
    jingle_send_iq_error(jn->message, "cancel", "service-unavailable", NULL);
    jingle_free_jinglenode(jn);
    g_free(disp);
    return;
  }
  
  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    jingle_free_jinglenode(jn);
    g_free(disp);
    return;
  }

  // a session-initiate message must contains at least one <content> element
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    jingle_free_jinglenode(jn);
    g_free(disp);
    return;
  }
  
  /* When sending a session-initiate at least one <content/> element MUST have
   * a disposition of "session"); if this rule is violated, the responder MUST
   * return a <bad-request/> error to the initiator.
   */
  for (child = jn->content; child && !valid_disposition; child = child->next) {
    cn = (JingleContent*)(child->data);
    if (g_strcmp0(cn->disposition, "session") == 0 || cn->disposition == NULL)
      valid_disposition = TRUE;
  }
  if (!valid_disposition) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    g_free(disp);
    jingle_free_jinglenode(jn);
    return;  
  }
  
  // if a session with the same sid already exists
  if (session_find(jn) != NULL) {
    jingle_send_iq_error(jn->message, "cancel", "unexpected-request", "out-of-order");
    g_free(disp);
    jingle_free_jinglenode(jn);
    return;
  }

  jingle_ack_iq(jn->message);

  // We create a session
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
    
    session_add_content_from_jinglecontent(sess, cn,
                                           JINGLE_SESSION_STATE_PENDING);
  }
  
  if(g_slist_length(sess->content) == 0) {
    jingle_send_session_terminate(sess, "unsupported-applications");
    session_delete(sess);
    return;
  }
  
  // Wait that user accept the jingle
  sbuf = g_string_new("");
  g_string_printf(sbuf, "Received an invitation for a jingle session from <%s>",
                  lm_message_get_from(jn->message));

  scr_WriteIncomingMessage(disp, sbuf->str, 0, HBB_PREFIX_INFO, 0);
  scr_LogPrint(LPRINT_LOGNORM, "%s", sbuf->str);

  {
    const char *id;
    char *desc = g_strdup_printf("<%s> invites you to do a jingle session",
                                 lm_message_get_from(jn->message));

    id = evs_new(desc, NULL, 0, evscallback_jingle, sess, NULL);
    g_free(desc);
    if (id)
      g_string_printf(sbuf, "Please use /event %s accept|reject", id);
    else
      g_string_printf(sbuf, "Unable to create a new event!");
    scr_WriteIncomingMessage(disp, sbuf->str, 0, HBB_PREFIX_INFO, 0);
    scr_LogPrint(LPRINT_LOGNORM, "%s", sbuf->str);
  }
  g_free(disp);
  jingle_free_jinglenode(jn);
}

void handle_session_info(JingleNode *jn)
{
  JingleSession *sess;
  SessionContent *sc;
  GSList *el;

  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }

  /* "If either party receives an empty session-info message
   * for an active session, it MUST send an empty IQ result;
   * this usage functions as a "ping" to determine session
   * vitality via the XMPP signalling channel." */
  if (jn->node->children == NULL) {
    jingle_ack_iq(jn->message);
    return;
  }

  /* We don't know the app to which the message is addressed.
   * Only the app module itself may know, based on the element
   * name and its xmlns, so we need to try every app module. */
  for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*)el->data;
    if (sc->appfuncs->handle == NULL)
      continue;

    if (sc->appfuncs->handle(JINGLE_SESSION_INFO, sc->description,
                             jn->node->children)) {
      jingle_ack_iq(jn->message);
      return;
	}
  }
  /* "If the party that receives an informational message
   * does not understand the payload, it MUST return a
   * <feature-not-implemented/> error with a Jingle-specific
   * error condition of <unsupported-info/>." */
  jingle_send_iq_error(jn->message, "modify", "feature-not-implemented",
                       "unsupported-info");
  jingle_free_jinglenode(jn);
}

void handle_session_accept(JingleNode *jn)
{
  JingleSession *sess;
  JingleContent *jc;
  SessionContent *sc;
  session_content *sc2 = g_new0(session_content, 1);
  
  GSList *el;
  const gchar *from = lm_message_get_from(jn->message);
  
  // We're looking if the session exist
  sess = session_find_by_sid(jn->sid, from);
  if (sess == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }
  
  jingle_ack_iq(jn->message);
  
  for (el = jn->content; el; el = el->next) {
    jc = (JingleContent*)el->data;
    session_changestate_sessioncontent(sess, jc->name,
                                       JINGLE_SESSION_STATE_ACTIVE);
  }
  
  // We delete content who have been refuse
  /*for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*) el->data;
    if (sc->state == JINGLE_SESSION_STATE_PENDING) {
      scr_log_print(LPRINT_DEBUG, "Delete %s!", sc->name);
      session_remove_sessioncontent(sess, sc->name);
    }
  }*/
  
  // Go go go! We start jobs!
  for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*)el->data;
    sc2->sid = sess->sid;
    sc2->from = (sess->origin == JINGLE_SESSION_INCOMING) ? sess->from : sess->to;
    sc2->name = sc->name;
    sc->appfuncs->start(sc2);
  }
  jingle_free_jinglenode(jn);
}

void handle_session_terminate(JingleNode *jn)
{
  JingleSession *sess;
  GSList *el;
  SessionContent *sc;
  
  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }
  
  for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*)el->data;
    if (!g_strcmp0(lm_message_get_from(jn->message),
             (sess->origin == JINGLE_SESSION_INCOMING) ? sess->from : sess->to))
      sc->appfuncs->stop(sc->description);
    session_remove_sessioncontent(sess, sc->name);
  }
  session_delete(sess);
  jingle_ack_iq(jn->message);
  jingle_free_jinglenode(jn);
}
