#ifndef __JINGLE_SESSIONS_H__
#define __JINGLE_SESSIONS_H__ 1

#include <glib.h>


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
  gconstpointer *description;
  JingleAppFuncs *appfuncs;
  gconstpointer *transport;
  JingleTransportFuncs *transfuncs;
} SessionContent;


JingleSession *session_new(JingleNode *jn, LmMessageNode *app,
                           LmMessageNode *transport);
JingleSession *session_find_by_sid(const gchar *sid, const gchar *from);
JingleSession *session_find(const JingleNode *jn);
void session_delete(JingleSession *sess);
void session_remove(JingleSession *sess);
void session_free(JingleSession *sess);

#endif
