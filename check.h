#ifndef __JINGLE_CHECK_H__
#define __JINGLE_CHECK_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include "jingle.h"

#define JINGLE_CHECK_ERROR jingle_check_error_quark()


typedef enum {
  JINGLE_CHECK_ERROR_MISSING,  // an element or attribute is missing
  JINGLE_CHECK_ERROR_BADELEM,  // an element is not where it is supposed to be
  JINGLE_CHECK_ERROR_BADVALUE  // the value of an attribute is incorrect
} JingleCheckError;


int check_jingle(LmMessageNode* node, JingleNode *jd, GError **err);
GQuark jingle_check_error_quark();

#endif
