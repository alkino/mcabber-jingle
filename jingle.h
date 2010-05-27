#ifndef __JINGLE_H__
#define __JINGLE_H__ 1

#define NS_JINGLE "urn:xmpp:jingle:1"
#define NS_JINGLE_ERRORS "urn:xmpp:jingle:errors:1"

void jingle_error_iq(LmMessage *m, const gchar *errtype,
                     const gchar *cond, const gchar *jinglecond);

void jingle_ack_iq(LmMessage *m);

typedef enum {
  JINGLE_UNKNOWN,
  JINGLE_CONTENT_ACCEPT,
  JINGLE_CONTENT_ADD,
  JINGLE_CONTENT_MODIFY,
  JINGLE_CONTENT_REJECT,
  JINGLE_CONTENT_REMOVE,
  JINGLE_DESCRIPTION_INFO,
  JINGLE_SECURITY_INFO,
  JINGLE_SESSION_ACCEPT,
  JINGLE_SESSION_INFO,
  JINGLE_SESSION_INITIATE,
  JINGLE_SESSION_TERMINATE,
  JINGLE_TRANSPORT_ACCEPT,
  JINGLE_TRANSPORT_INFO,
  JINGLE_TRANSPORT_REJECT,
  JINGLE_TRANSPORT_REPLACE,
} JingleAction;

#endif
