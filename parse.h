#ifndef __PARSE_H__
#define __PARSE_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#define PARSE_OK                    0
#define PARSE_ERROR_NAME            1
#define PARSE_ERROR_REQUIRED        2
#define PARSE_ERROR_RESTRICTION     3
#define PARSE_ERROR_TOO_MANY_CHILDS 4


struct iq_data {
  const gchar *from;
  const gchar *id;
  const gchar *to;
  const gchar *set;
};

struct jingle_data {
  const gchar* action;    // see jingle_action in jingle.h
  const gchar* initiator; // optional
  const gchar* responder;  // optional
  const gchar* sid;        // required
};

struct content_data {
  const gchar* creator;     // required (initiator, responder)
  const gchar* disposition; // optional, default=session
  const gchar* name;        // required
  const gchar* senders;     // optional (both, initiator, none, responder)
};


int parse_jingle(LmMessageNode* node, struct jingle_data* ij);
int parse_content(LmMessageNode* node, struct content_data* ic);

gint str_in_array(const gchar* needle, const gchar** haystack);

#endif
