#ifndef __JINGLE_H__
#define __JINGLE_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#define NS_JINGLE "urn:xmpp:jingle:1"
#define NS_JINGLE_ERRORS "urn:xmpp:jingle:errors:1"


typedef enum {
  JINGLE_UNKNOWN_ACTION,
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

typedef enum {
  JINGLE_CREATOR_INITIATOR,
  JINGLE_CREATOR_RESPONDER,
} JingleCreator;

typedef enum {
  JINGLE_SENDERS_BOTH,
  JINGLE_SENDERS_INITIATOR,
  JINGLE_SENDERS_NONE,
  JINGLE_SENDERS_RESPONDER,
} JingleSenders;

typedef struct {
  /* pointer to the original LmMessage */
  LmMessage *message;

  /* poiter to the <jingle> element */
  LmMessageNode *node;

  /* action attribute */
  JingleAction action;

  /* full JID of the entity that has initiated the session flow.
   * may be different from the 'from' address on the IQ-set of the
   * session-initiate message.
   * recommended for session-initiate, not recommended otherwise. */
  const gchar *initiator;

  /* full JID of the entity that has replied to the initation.
   * can be different from the 'to' address on the IQ-set.
   * recommended for session-accept, not recommended otherwise. */
  const gchar *responder;

  /* Random session identifier generated by the initator. */
  const gchar *sid;

  /* Linked list of JingleContent. */
  GSList *content;

} JingleNode;

typedef struct {
  /* pointer to the <content> element */
  LmMessageNode *node;

  /* which party originally generated the content type.
   * the defined values are "initiator" and "responder"
   * (where the default is "initiator"). required. */
  JingleCreator creator;

  /* how the content definition is to be interpreted by the recipient.
   * optional, the default value is "session". */
  const gchar *disposition; // optional, default=session

  /* A unique name or identifier for the content type
   * according to the creator. required.*/
  const gchar *name;

  /* which parties in the session will be generating content.
   * allowable values are both, initiator, none, responder.
   * default is both.
   * required for content-modify, optional otherwise */
  JingleSenders senders;

  /* each content element (must) contain one description
   * child element that specifies a desired application.
   * the content of this node is app specific. */
  gconstpointer *description;

  /* each content element (must) contain one transport
   * child element that specifies a potential transport
   * method */
  gconstpointer *transport;

} JingleContent;

struct JingleActionList {
  const gchar  *name;
  void (*handler)(LmMessage *, JingleNode *);
};


LmMessage *jingle_new_iq_error(LmMessage *m, const gchar *errtype,
                               const gchar *cond, const gchar *jinglecond);
void jingle_send_iq_error(LmMessage *m, const gchar *errtype,
                          const gchar *cond, const gchar *jinglecond);
void jingle_ack_iq(LmMessage *m);
JingleAction jingle_action_from_str(const gchar* string);

#endif
