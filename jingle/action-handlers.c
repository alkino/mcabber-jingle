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


void handle_content_accept(JingleNode *jn)
{
  GError *err = NULL;
  GSList *child = NULL;
  JingleContent *cn;
  JingleAppFuncs *appfuncs;
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  JingleSession *sess;

  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  // it's better if there is at least one content elem
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }
  
  // if a session with the same sid doesn't already exists
  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }

  jingle_ack_iq(jn->message);

  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);
    session_changestate_sessioncontent(sess, cn->name, JINGLE_SESSION_STATE_ACTIVE);
  }
}

void handle_content_add(JingleNode *jn)
{
  GError *err = NULL;
  GSList *child = NULL;
  JingleContent *cn;
  JingleAppFuncs *appfuncs; 
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  JingleSession *sess;
  JingleNode accept, reject;
  JingleContent tmp_cn;
  LmMessage *r;

  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }

  jingle_ack_iq(jn->message);

  accept.action = JINGLE_CONTENT_ACCEPT;
  accept.sid = jn->sid;
  accept.content = NULL;
  
  reject.action = JINGLE_CONTENT_REJECT;
  reject.sid = jn->sid;
  reject.content = NULL;
  
  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);

    xmlns = lm_message_node_get_attribute(cn->description, "xmlns");
    appfuncs = jingle_get_appfuncs(xmlns);
    if (appfuncs == NULL) continue;
    
    xmlns = lm_message_node_get_attribute(cn->transport, "xmlns");
    transfuncs = jingle_get_transportfuncs(xmlns);
    if (appfuncs == NULL) continue;
    
    description = appfuncs->check(cn, &err);
    if (description == NULL || err != NULL) {
      reject.content = g_slist_append(reject.content, cn);
      continue;
    }
    transport = transfuncs->check(cn, &err);
    if (transport == NULL || err != NULL) {
      reject.content = g_slist_append(reject.content, cn);
      continue;
    }
    session_add_content_from_jinglecontent(sess, cn, JINGLE_SESSION_STATE_ACTIVE);
    accept.content = g_slist_append(accept.content, cn);
  }
  
  if (g_slist_length(accept.content) != 0) {
    JingleAckHandle *ackhandle = g_new0(JingleAckHandle, 1);
    r = lm_message_from_jinglenode(&accept, lm_message_get_from(jn->message));
    if (r) {
      ackhandle->callback = NULL;
      ackhandle->user_data = NULL;
      lm_connection_send_with_reply(lconnection, r,
                                    jingle_new_ack_handler(ackhandle), &err);
      lm_message_unref(r);
      if (err != NULL) {
        // TODO
      }
    }
  }
  
  if (g_slist_length(reject.content) != 0) {
    JingleAckHandle *ackhandle = g_new0(JingleAckHandle, 1);
    r = lm_message_from_jinglenode(&reject, lm_message_get_from(jn->message));
    if (r) {
      ackhandle->callback = NULL;
      ackhandle->user_data = NULL;
      lm_connection_send_with_reply(lconnection, r,
                                    jingle_new_ack_handler(ackhandle), &err);
      lm_message_unref(r);
      if (err != NULL) {
        // TODO
      }
    }
  }
}

void handle_content_reject(JingleNode *jn)
{
  GError *err = NULL;
  GSList *child = NULL;
  JingleContent *cn;
  JingleAppFuncs *appfuncs; 
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  JingleSession *sess;

  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }

  jingle_ack_iq(jn->message);

  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);
    session_remove_sessioncontent(sess, cn->name);
  }
  
  // If there is nothing more to do, close the session
  if (g_slist_length(sess->content) == 0) {
    jingle_send_session_terminate(sess, "success");
    session_delete(sess);
    return;
  }
}

void handle_content_remove(JingleNode *jn)
{
  GError *err = NULL;
  GSList *child = NULL;
  JingleContent *cn;
  JingleAppFuncs *appfuncs; 
  JingleTransportFuncs *transfuncs;
  gconstpointer description, transport;
  const gchar *xmlns;
  JingleSession *sess;

  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }

  /* it's better if there is at least one content elem */
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    return;
  }
  
  // if a session with the same sid doesn't already exists
  if ((sess = session_find(jn)) == NULL) {
    jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }

  jingle_ack_iq(jn->message);

  for (child = jn->content; child; child = child->next) {
    cn = (JingleContent *)(child->data);
    session_remove_sessioncontent(sess, cn->name);
  }
}

void handle_session_initiate(JingleNode *jn)
{
  GError *err = NULL;
  gboolean valid_disposition = FALSE;
  JingleContent *cn;
  GString *sbuf;
  GSList *child = NULL;
  LmMessage *r;
  gchar *disp;
  
  // Make sure the request come from an user in our roster
  disp = jidtodisp(lm_message_get_from(jn->message));
  if (!roster_find(disp, jidsearch, 0)) {
    // jingle_send_session_terminate(jn, "decline");
    jingle_free_jinglenode(jn);
    g_free(disp);
    return;
  }
  
  if (!check_contents(jn, &err)) {
    scr_log_print(LPRINT_DEBUG, "jingle: One of the content element was invalid (%s)",
                  err->message);
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
    g_free(disp);
    return;
  }

  // a session-initiate message must contains at least one <content> element
  if (g_slist_length(jn->content) < 1) {
    jingle_send_iq_error(jn->message, "cancel", "bad-request", NULL);
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
    return;  
  }
  
  // if a session with the same sid already exists
  if (session_find(jn) != NULL) {
    jingle_send_iq_error(jn->message, "cancel", "unexpected-request", "out-of-order");
    g_free(disp);
    return;
  }

  jingle_ack_iq(jn->message);

  // Wait that user accept the jingle
  sbuf = g_string_new("");
  g_string_printf(sbuf, "Received an invitation for a jingle session from <%s>", lm_message_get_from(jn->message));

  scr_WriteIncomingMessage(disp, sbuf->str, 0, HBB_PREFIX_INFO, 0);
  scr_LogPrint(LPRINT_LOGNORM, "%s", sbuf->str);

  {
    const char *id;
    char *desc = g_strdup_printf("<%s> invites you to do a jingle session", lm_message_get_from(jn->message));

    id = evs_new(desc, NULL, 0, evscallback_jingle, jn, NULL);
    g_free(desc);
    if (id)
      g_string_printf(sbuf, "Please use /event %s accept|reject", id);
    else
      g_string_printf(sbuf, "Unable to create a new event!");
    scr_WriteIncomingMessage(disp, sbuf->str, 0, HBB_PREFIX_INFO, 0);
    scr_LogPrint(LPRINT_LOGNORM, "%s", sbuf->str);
  }
  g_free(disp);
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
    // TODO size!
    sc2->sid = sess->sid;
    sc2->from = (sess->origin == JINGLE_SESSION_INCOMING) ? sess->from : sess->to;
    sc2->name = sc->name;
    sc->appfuncs->start(sc2, 2048);
  }

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
    sc->appfuncs->stop(sc->description);
    session_remove_sessioncontent(sess, sc->name);
  }
  session_delete(sess);
  jingle_ack_iq(jn->message);
}
