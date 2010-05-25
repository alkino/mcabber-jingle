#ifndef __JINGLE_H__
#define __JINGLE_H__ 1

#define NS_JINGLE "urn:xmpp:jingle:1"
#define NS_JINGLE_ERRORS "urn:xmpp:jingle:errors:1"

void jingle_error_iq(LmMessage *m, const gchar *errtype,
                     const gchar *cond, const gchar *jinglecond);

void jingle_ack_iq(LmMessage *m);

#endif
