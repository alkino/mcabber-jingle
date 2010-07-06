#ifndef __JINGLE_SESSIONS_H__
#define __JINGLE_SESSIONS_H__ 1

#include <glib.h>

#include <jingle/register.h>


typedef enum {
  JINGLE_SESSION_
} JingleStatus;

typedef struct {
  JingleStatus  status;
  gchar *sid;
  gchar *initiator;
  gchar *from;
  GSList *content;
} JingleSession;

typedef struct {
  gconstpointer description;
  JingleAppFuncs *appfuncs;
  gconstpointer transport;
  JingleTransportFuncs *transfuncs;
} SessionContent;


JingleSession *session_new(JingleNode *jn);
JingleSession *session_find_by_sid(const gchar *sid, const gchar *from);
JingleSession *session_find(const JingleNode *jn);
void session_add_content(JingleSession *sess, JingleContent *cn);
SessionContent *session_find_sessioncontent(JingleSession *sess, gconstpointer desc, gconstpointer trans);
void session_delete(JingleSession *sess);
void session_remove(JingleSession *sess);
void session_free(JingleSession *sess);

#endif
