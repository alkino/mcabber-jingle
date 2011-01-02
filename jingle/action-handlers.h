#ifndef __JINGLE_ACTION_HANDLERS_H__
#define __JINGLE_ACTION_HANDLERS_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

void handle_content_accept(JingleNode *jn);
void handle_content_add(JingleNode *jn);
void handle_content_reject(JingleNode *jn);
void handle_content_remove(JingleNode *jn);
void handle_session_accept(JingleNode *jn);
void handle_session_info(JingleNode *jn);
void handle_session_initiate(JingleNode *jn);
void handle_session_terminate(JingleNode *jn);
void handle_transport_info(JingleNode *jn);
void handle_transport_initialize(int correct, session_content *sc2);

#endif
