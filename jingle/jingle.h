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

  /* pointer to the <jingle> element */
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
   * the connt of this node is app specific. */
  LmMessageNode *description;

  /* each content element (must) contain one transport
   * child element that specifies a potential transport
   * method */
  LmMessageNode *transport;
} JingleContent;

struct JingleActionList {
  const gchar  *name;
  void (*handler)(JingleNode *);
};

typedef enum {
  JINGLE_ACK_RESPONSE,
  JINGLE_ACK_TIMEOUT
} JingleAckType;

typedef void (*JingleAckCallback) (JingleAckType type, LmMessage *, gpointer);

typedef struct {
  /* function to be called when we receive a response to the IQ */
  JingleAckCallback callback;

  /* additional data to pass to callback */
  gpointer *user_data;

  /* if no response was received after timeout seconds, callback
   * will be called with JINGLE_ACK_TIMEOUT as type */
  guint timeout;

  /* (private) date at which the handler was inserted using 
   * jingle_new_ack_handler */
  time_t _inserted;
  
  /* (private) a pointer to the LmMessageHandler created
   * using jingle_new_ack_handler */
  LmMessageHandler *_handler;
} JingleAckHandle;

typedef struct {
  const gchar *sid;
  const gchar *from;
  const gchar *name;
} session_content;

LmHandlerResult jingle_handle_ack_iq(LmMessageHandler *handler,
                                     LmConnection *connection, 
                                     LmMessage *message, gpointer user_data);
LmMessageHandler *jingle_new_ack_handler(JingleAckHandle *ri);
void jingle_ack_handler_free(JingleAckHandle *ah);

LmMessage *jingle_new_iq_error(LmMessage *m, const gchar *errtype,
                               const gchar *cond, const gchar *jinglecond);
void jingle_send_iq_error(LmMessage *m, const gchar *errtype,
                          const gchar *cond, const gchar *jinglecond);

gchar *jingle_find_compatible_res(const gchar *jid, const gchar *ns[]);

void jingle_ack_iq(LmMessage *m);

void jingle_free_jinglenode(JingleNode *jn);

JingleAction jingle_action_from_str(const gchar* string);

gboolean evscallback_jingle(guint evcontext, const gchar *arg,
                            gpointer userdata);
                            
void handle_trans_data(gconstpointer data, const gchar *data2, guint len);

gchar *jingle_generate_sid(void);

void handle_trans_next(session_content *sc2);

#endif
