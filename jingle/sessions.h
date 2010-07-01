#ifndef __JINGLE_SESSIONS_H__
#define __JINGLE_SESSIONS_H__ 1

#include <glib.h>


typedef enum {
  JINGLE_SESSION_
} JingleStatus;

typedef struct {
  JingleStatus  status;
  const gchar *sid;
  const gchar *from;
  GSList *content;
} JingleSession;


JingleSession *session_new(JingleNode *jn,
                           LmMessageNode *app,
                           LmMessageNode *transport);
JingleSession *session_find();
void session_delete(JingleSession *sess);
void session_remove(JingleSession *sess);
void session_free(JingleSession *sess);

#endif
