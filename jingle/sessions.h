#ifndef __JINGLE_SESSIONS_H__
#define __JINGLE_SESSIONS_H__ 1

#include <glib.h>

#include <jingle/register.h>


typedef enum {
  JINGLE_SESSION_STATE_ACTIVE,
  JINGLE_SESSION_STATE_PENDING,
  JINGLE_SESSION_STATE_ENDED,
} SessionState;

typedef enum {
  /* A session that was initiated by some remote entity */
  JINGLE_SESSION_INCOMING,

  /* A session that we initiated (by sending a session-initiate) */
  JINGLE_SESSION_OUTGOING
} SessionOrigin;

typedef struct {
  /* Random session identifier generated by the initator. */
  gchar *sid;

  /* We need to know who initiated the session ?
   * us or a remote entity. */
  SessionOrigin origin;

  /* jid of the xmpp entity that initiated the session. */
  gchar *from;

  /* jid of the xmpp entity that received the initiation request. */
  gchar *to;

  /* jid of the entity we send iq. point to "from" or "to" depending on
   * who initiated the session. Basically a shortcut to avoid checking
   * origin. Should not be freed when destroying a JingleSession. */
  gchar *recipient;

  /* A singly-linked list of content. */
  GSList *content;
} JingleSession;

typedef struct {
  /* "A unique name or identifier for the content type
   *  according to the creator" */
  gchar *name;

  /* */
  SessionState state;

  /* The namespace of the app */
  gchar *xmlns_desc;

  /* The internal struct of the app module */
  gconstpointer description;

  /* Struct of functions provided by the app module */
  JingleAppFuncs *appfuncs;

  /* The namespace of the transport */
  gchar *xmlns_trans;

  /* The internal struct of the transport module */
  gconstpointer transport;

  /* Struct of functions provided by the transport module */
  JingleTransportFuncs *transfuncs;
} SessionContent;


// Manage sessions:
//    Inititiator:
void new_session_with_apps(const gchar *recipientjid, const gchar **name,
                           gconstpointer *datas, const gchar **ns);

//    Responder:
JingleSession *session_new_from_jinglenode(JingleNode *jn);
SessionContent *session_add_content_from_jinglecontent(JingleSession *sess,
                           JingleContent *cn, SessionState state, GError **err);

//    Both:
JingleSession *session_new(const gchar *sid, const gchar *from,
                           const gchar *to, SessionOrigin origin);
void session_delete(JingleSession *sess);
void session_remove(JingleSession *sess);
void session_free(JingleSession *sess);
SessionContent *session_add_content(JingleSession *sess, const gchar *name,
                                    SessionState state);

// Manage contents:
int session_remove_sessioncontent(JingleSession *sess, const gchar *name);
void session_changestate_sessioncontent(JingleSession *sess, const gchar *name,
                                        SessionState state);
void session_add_app(JingleSession *sess, const gchar *name,
                           const gchar *xmlns, gconstpointer data);
void session_add_trans(JingleSession *sess, const gchar *name,
                           const gchar *xmlns, gconstpointer data);


// Search:
//   Session:
JingleSession *session_find_by_sid(const gchar *sid, const gchar *from);
JingleSession *session_find(const JingleNode *jn);
JingleSession *session_find_by_sessioncontent(SessionContent *sc);
SessionContent *sessioncontent_find_by_app(gconstpointer data);
SessionContent *sessioncontent_find_by_transport(gconstpointer data);

//   Content:
SessionContent *sessioncontent_find_by_transport(gconstpointer data);
SessionContent *sessioncontent_find_by_app(gconstpointer data);
SessionContent *session_find_sessioncontent(JingleSession *sess,
                                            const gchar *name);

// API:
void jingle_handle_app(const gchar *name,
                       const gchar *xmlns_app, gconstpointer app,
                       const gchar *to);
LmMessage *lm_message_from_jinglesession(const JingleSession *js,
                                         JingleAction action);
void handle_app_data(const gchar *sid, const gchar* from, const gchar *name, gchar *data, gsize size);
#endif
