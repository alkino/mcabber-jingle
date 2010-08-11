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

#include <jingle/jingle.h>
#include <jingle/sessions.h>
#include <jingle/register.h>
#include <mcabber/logprint.h>

static GSList *sessions;

static void lm_insert_sessioncontent(gpointer data, gpointer userdata);

extern struct JingleActionList jingle_action_list[];

/**
 * Create a new session and insert it in the linked list.
 */
JingleSession *session_new(const gchar *sid, const gchar *from,
                           const gchar *to, SessionOrigin origin)
{
  JingleSession *js = g_new0(JingleSession, 1);
  
  js->sid  = g_strdup(sid);
  js->from = g_strdup(from);
  js->to   = g_strdup(to);
  js->origin = origin;

  sessions = g_slist_append(sessions, js);
  return js;
}

JingleSession *session_new_from_jinglenode(JingleNode *jn)
{
  const gchar *from, *to;
  
  from = lm_message_node_get_attribute(jn->message->node, "from");
  to = lm_message_node_get_attribute(jn->message->node, "to");

  if (!from || !to) {
    return NULL;
  }

  return session_new(jn->sid, from, to, JINGLE_SESSION_INCOMING);
}

JingleSession *session_find_by_sid(const gchar *sid, const gchar *from)
{
  GSList *el;
  JingleSession *js;
  gchar *recipient;
  for (el = sessions; el; el = el->next) {
    js = (JingleSession*) el->data;
    recipient = (js->origin == JINGLE_SESSION_INCOMING) ? js->from : js->to;
    if (!g_strcmp0(js->sid, sid) && !g_strcmp0(recipient, from)) {
      return js;
    }
  }
  return NULL;
}

JingleSession *session_find(const JingleNode *jn)
{
  const gchar *from = lm_message_node_get_attribute(jn->message->node, "from");
  return session_find_by_sid(jn->sid, from);
}

void session_add_content(JingleSession *sess, const gchar *name,
                         SessionState state)
{
  SessionContent *sc = g_new0(SessionContent, 1);
  
  sc->name = name;
  sc->state = state;

  sess->content = g_slist_append(sess->content, sc);
}

void session_add_app(JingleSession *sess, const gchar *name,
                           const gchar *xmlns, gconstpointer data)
{
  SessionContent *sc = session_find_sessioncontent(sess, name);
  
  sc->xmlns_desc = xmlns;
  sc->appfuncs = jingle_get_appfuncs(xmlns);
  sc->description = data;
}

void session_add_trans(JingleSession *sess, const gchar *name,
                           const gchar *xmlns, gconstpointer data)
{
  SessionContent *sc = session_find_sessioncontent(sess, name);
  
  sc->xmlns_trans = xmlns;
  sc->transfuncs = jingle_get_transportfuncs(xmlns);
  sc->transport = data;
}

void session_add_content_from_jinglecontent(JingleSession *sess, JingleContent *cn,
                         SessionState state)
{
  const gchar *xmlns;
  JingleAppFuncs *app_funcs;
  JingleTransportFuncs *trans_funcs;
  session_add_content(sess, cn->name, state);
  
  xmlns = lm_message_node_get_attribute(cn->description, "xmlns");
  app_funcs = jingle_get_appfuncs(xmlns);
  session_add_app(sess, cn->name, xmlns, app_funcs->check(cn, NULL));
  
  xmlns = lm_message_node_get_attribute(cn->transport, "xmlns");
  trans_funcs = jingle_get_transportfuncs(xmlns);
  session_add_trans(sess, cn->name, xmlns, trans_funcs->check(cn, NULL));
}

SessionContent *session_find_sessioncontent(JingleSession *sess,
                                            const gchar *name)
{
  GSList *el;
  SessionContent *sc;
  for (el = sess->content; el; el = el->next) {
    sc = (SessionContent*) el->data;
    if (!g_strcmp0(sc->name, name))
      return sc;
  }
  return NULL;
}

SessionContent *session_find_transport(const gchar *xmlns_trans, gconstpointer data)
{
  GSList *el, *el1;
  JingleSession *sess;
  SessionContent *sc;
  for (el1 = sessions; el1; el1 = el1->next) {
    sess = (JingleSession*) el1->data;
    for (el = sess->content; el; el = el->next) {
      sc = (SessionContent*) el->data;
      if (!g_strcmp0(sc->xmlns_trans, xmlns_trans) && sc->transfuncs->cmp(sc->transport, data))
        return sc;
    }
  }
  return NULL;
}

void session_remove_sessioncontent(JingleSession *sess, const gchar *name)
{
  SessionContent *sc;
  sc = session_find_sessioncontent(sess, name);
  if(sc == NULL) return;

  if (sc->state == JINGLE_SESSION_STATE_ACTIVE); // We should stop the transfert

  sess->content = g_slist_remove(sess->content, sc);
}

void session_changestate_sessioncontent(JingleSession *sess, const gchar *name,
                                        SessionState state)
{
  SessionContent *sc;
  sc = session_find_sessioncontent(sess, name);
  if(sc != NULL)
    sc->state = state;
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

void jingle_handle_app(JingleSession *sess, const gchar *name,
                       const gchar *xmlns_app, gconstpointer app,
                       const gchar *to)
{
  JingleTransportFuncs *trans = jingle_transport_for_app(xmlns_app, NULL);
  
  if (trans == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Unable to find a transport for %s", xmlns_app);
    return;
  }
  
  session_add_trans(sess, name, trans->xmlns(), trans->new());

  jingle_send_session_initiate(sess);
}

LmMessage *lm_message_from_jinglesession(const JingleSession *js,
                                         JingleAction action)
{
  LmMessage* m; 
  LmMessageNode *jnode;
  const gchar *actionstr, *recipient;
  
  recipient = (js->origin == JINGLE_SESSION_INCOMING) ? js->from : js->to;

  m = lm_message_new_with_sub_type(recipient, LM_MESSAGE_TYPE_IQ,
                                   LM_MESSAGE_SUB_TYPE_SET);

  jnode = lm_message_node_add_child(m->node, "jingle", NULL);

  lm_message_node_set_attribute(jnode, "xmlns", NS_JINGLE);
  
  if (actionstr = jingle_action_list[action].name)
    lm_message_node_set_attribute(jnode, "action", actionstr);
  else
    return NULL;

  if (js->sid)
    lm_message_node_set_attribute(jnode, "sid", js->sid);
  else
    return NULL;

  g_slist_foreach(js->content, lm_insert_sessioncontent, jnode);
  return m;
}

static void lm_insert_sessioncontent(gpointer data, gpointer userdata)
{
  const gchar *xmlns;
  JingleTransportFuncs *tfunc;
  JingleAppFuncs *afunc;
  SessionContent *content = (SessionContent*) data;
  LmMessageNode *jnode = (LmMessageNode*) userdata;
  LmMessageNode *node = lm_message_node_add_child(jnode, "content", NULL);
                                                                 
  if (content->name)
    lm_message_node_set_attribute(node, "name", content->name);
  
  content->transfuncs->tomessage(content->transport, node);

  content->appfuncs->tomessage(content->description, node);
}

void handle_app_data(JingleSession *sess, SessionContent *sc, gchar *data, gsize size)
{
  // TODO: verify that the module is always loaded
  sc->transfuncs->send(sess->to, sc->transport, data, size);
}
