#ifndef __PARSE_H__
#define __PARSE_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include "jingle.h"

#define JINGLE_CHECK_ERROR jingle_check_error_quark()


typedef enum {
  JINGLE_CHECK_ERROR_MISSING,  // an element or attribute is missing
  JINGLE_CHECK_ERROR_BADELEM,  // an element is not where it is supposed to be
  JINGLE_CHECK_ERROR_BADVALUE  // the value of an attribute is incorrect
} JingleCheckError;

typedef struct {
  JingleAction action;
  const gchar* initiator; // optional
  const gchar* responder; // optional
  const gchar* sid;       // required
} JingleData;

typedef struct {
  const gchar* creator;     // required (initiator, responder)
  const gchar* disposition; // optional, default=session
  const gchar* name;        // required
  const gchar* senders;     // optional (both, initiator, none, responder)
} ContentData;


int check_jingle(LmMessageNode* node, JingleData *jd, GError **err);
GQuark jingle_check_error_quark();

#endif
