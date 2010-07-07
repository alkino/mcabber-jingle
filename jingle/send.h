#ifndef __SEND_H__
#define __SEND_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>


void jingle_send_session_terminate(JingleNode *jn, const gchar *reason);

#endif
