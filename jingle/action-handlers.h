#ifndef __JINGLE_ACTION_HANDLERS_H__
#define __JINGLE_ACTION_HANDLERS_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

void handle_content_accept(LmMessage *m, JingleNode *jn);
void handle_content_add(LmMessage *m, JingleNode *jn);
void handle_content_reject(LmMessage *m, JingleNode *jn);
void handle_content_remove(LmMessage *m, JingleNode *jn);
void handle_session_initiate(LmMessage *m, JingleNode *jn);
void handle_session_terminate(LmMessage *m, JingleNode *jn);
#endif
