#ifndef __SEND_H__
#define __SEND_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include <jingle/jingle.h>
#include <jingle/sessions.h>

void jingle_send_session_terminate(JingleSession *js, const gchar *reason);
void jingle_send_session_initiate(JingleSession *js);
void jingle_send_session_accept(JingleNode *jn);

#endif
