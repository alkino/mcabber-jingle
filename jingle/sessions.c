/*
 * sessions.c
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

#include <mcabber/xmpp_helper.h>

#include <jingle/jingle.h>
#include <jingle/sessions.h>
#include <jingle/register.h>


static GSList *sessions;


/**
 * Create a new session and insert it in the linked list.
 */
JingleSession *session_new(JingleNode *jn)
{
  JingleSession *js = g_new0(JingleSession, 1);
  const gchar *from;
  
  js->sid = g_strdup(jn->sid);
  js->initiator = g_strdup(jn->initiator);
  from = lm_message_get_from(jn->message);
  if (!from) {
    return NULL;
  }
  js->from = g_strdup(from);

  sessions = g_slist_append(sessions, js);
  return js;
}

JingleSession *session_find_by_sid(const gchar *sid, const gchar *from)
{
  GSList *el;
  JingleSession *js;
  for (el = sessions; el; el = el->next) {
    js = (JingleSession*) el->data;
    if (g_strcmp0(js->sid, sid) && g_strcmp0(js->from, from)) {
      return js;
    }
  }
  return NULL;
}

JingleSession *session_find(const JingleNode *jn)
{
  const gchar *from = lm_message_get_from(jn->message);
  return session_find_by_sid(jn->sid, from);
}

void session_add_content(JingleSession *sess, JingleContent *cn)
{
  SessionContent *sc = g_new0(SessionContent, 1);
  const gchar *tmpchar = lm_message_node_get_attribute(cn->description,
                                                       "xmlns");
  sc->appfuncs = jingle_get_appfuncs(tmpchar);
  tmpchar = lm_message_node_get_attribute(cn->transport, "xmlns");
  sc->transfuncs = jingle_get_transportfuncs(tmpchar);
  // TODO errors
  sc->description = sc->appfuncs->check(cn, NULL);
  sc->transport = sc->appfuncs->check(cn, NULL);

  sess->content = g_slist_append(sess->content, sc);
}

SessionContent *session_find_sessioncontent(JingleSession *sess,
                                        gconstpointer desc, gconstpointer trans)
{
  GSList *el;
  SessionContent *sc;
  for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*) el->data;
    if (sc->appfuncs->cmp(sc->description, desc) == TRUE &&
        sc->transfuncs->cmp(sc->transport, trans) == TRUE)
      return sc;
  }
  return NULL;
}

/**
 * Remove a session from the linked list and free it.
 */
void session_delete(JingleSession *sess)
{
  session_remove(sess);
  session_free(sess);
}

/**
 * Remove a session from the linked list.
 */
void session_remove(JingleSession *sess)
{
  sessions = g_slist_remove(sessions, sess);
}

/**
 * Free a session.
 */
void session_free(JingleSession *sess)
{
  g_free(sess->sid);
  g_free(sess->from);
  g_free(sess);
}
