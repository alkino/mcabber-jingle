#ifndef __PARSE_H__
#define __PARSE_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#define PARSE_OK                    0
#define PARSE_ERROR_NAME            1
#define PARSE_ERROR_REQUIRED        2
#define PARSE_ERROR_RESTRICTION     3
#define PARSE_ERROR_TOO_MANY_CHILDS 4

struct info_iq {
  gchar* from;
  gchar* id;
  gchar* to;
  gchar* set;
};

struct info_jingle {
  gchar* action;      // required (content-accept, content-add,
  // content-modify, content-reject, content-remove, description-info
  // security-info, session-accept, session-info, session-initiate,
  // session-terminate, transport-accept, transport-info, transport-reject,
  // transport-replace)
  gchar* initiator;   // optional
  gchar* responder;   // optional
  gchar* sid;         // required
};

struct info_content {
  gchar* creator;     // required (initiator, responder)
  gchar* disposition; // optional, default=session
  gchar* name;        // required
  gchar* senders;     // optional (both, initiator, none, responder)
};

int parse_jingle(LmMessageNode* node, struct info_jingle* ij);
void free_jingle(struct info_jingle* ij);
int parse_content(LmMessageNode* node, struct info_content* ic);
void free_content(struct info_content* ic);

int check_restriction(const char* name, const char** values);

#endif
