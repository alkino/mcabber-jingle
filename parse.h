#include <loudmouth/loudmouth.h>

#define PARSE_OK                    0
#define PARSE_ERROR_NAME            1
#define PARSE_ERROR_REQUIRED        2
#define PARSE_ERROR_RESTRICTION     3
#define PARSE_ERROR_TOO_MANY_CHILDS 4

struct info_iq {
  char* from;
  char* id;
  char* to;
  char* set;
};

struct info_jingle {
  char* action;      // required (content-accept, content-add,
  // content-modify, content-reject, content-remove, description-info
  // security-info, session-accept, session-info, session-initiate,
  // session-terminate, transport-accept, transport-info, transport-reject,
  // transport-replace)
  char* initiator;   // optional
  char* responder;   // optional
  char* sid;         // required
};

struct info_content {
  char* creator;     // required (initiator, responder)
  char* disposition; // optional, default=session
  char* name;        // required
  char* senders;     // optional (both, initiator, none, responder)
};

int parse_jingle(LmMessageNode* node, struct info_jingle* ij);
void free_jingle(struct info_jingle* ij);
int parse_content(LmMessageNode* node, struct info_content* ic);
void free_content(struct info_content* ic);

int check_restriction(const char* name, const char** values)

char* attrcpy(const char* attr);
