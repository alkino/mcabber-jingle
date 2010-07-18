#ifndef __JINGLE_SESSIONS_H__
#define __JINGLE_SESSIONS_H__ 1

#include <glib.h>

#include <jingle/register.h>


typedef enum {
  JINGLE_SESSION_
} JingleStatus;

typedef enum {
  JINGLE_SESSION_STATE_ACTIVE,
  JINGLE_SESSION_STATE_PENDING,
  JINGLE_SESSION_STATE_ENDED,
} SessionState;

typedef struct {
  JingleStatus  status;
  gchar *sid;
  gchar *initiator;
  gchar *from;
  GSList *content;
} JingleSession;

typedef struct {
  const gchar *name;
  SessionState state;
  const gchar *xmlns_desc;
  gconstpointer description;
  JingleAppFuncs *appfuncs;
  const gchar *xmlns_trans;
  gconstpointer transport;
  JingleTransportFuncs *transfuncs;
} SessionContent;

JingleSession *session_new(JingleNode *jn);
JingleSession *session_find_by_sid(const gchar *sid, const gchar *from);
JingleSession *session_find(const JingleNode *jn);
void session_add_content(JingleSession *sess, JingleContent *cn, SessionState state);
SessionContent *session_find_sessioncontent(JingleSession *sess, const gchar *name);
SessionContent *session_find_transport(const gchar *xmlns_trans, gconstpointer data);
void session_remove_sessioncontent(JingleSession *sess, const gchar *name);
void session_changestate_sessioncontent(JingleSession *sess, const gchar *name,
                                        SessionState state);
void session_delete(JingleSession *sess);
void session_remove(JingleSession *sess);
void session_free(JingleSession *sess);

#endif
