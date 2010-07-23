#ifndef __JINGLE_REGISTER_H__
#define __JINGLE_REGISTER_H__

#include <jingle/jingle.h>


#define NS_JINGLE_APP_PREFIX       "urn:xmpp:jingle:apps:"
#define NS_JINGLE_TRANSPORT_PREFIX "urn:xmpp:jingle:transports:"


typedef enum {
  /* A datagram transport has one or more components with which to exchange
   * packets with UDP-like behavior. Packets might be of arbitrary length,
   * might be received out of order, and might not be received at all
   * (i.e., the transport is lossy). */
  JINGLE_TRANSPORT_STREAMING,
  
  /* A streaming transport has one or more components with which to exchange
   * bidirectional bytestreams with TCP-like behavior. Bytes are received
   * reliably and in order, and applications MUST NOT rely on a stream being
   * chunked in any specific way. */
  JINGLE_TRANSPORT_DATAGRAM
} JingleTransportType;

/**
 * We need to rank transports to determine which one to choose.
 * With this system, In-Band Bytestreams could have a low priority, SOCKS5
 * Bytestream a normal priority, and some stream transport method that allow
 * direct connection would have a high priority, since it would be the fastest.
 */
typedef enum {
  JINGLE_TRANSPORT_LOW,
  JINGLE_TRANSPORT_NORMAL,
  JINGLE_TRANSPORT_HIGH
} JingleTransportPriority;

typedef gconstpointer (*JingleAppCheck) (JingleContent *cn, GError **err);
typedef void (*JingleAppHandle) (JingleAction action, gconstpointer data, LmMessageNode *node);
typedef gboolean (*JingleAppHandleData) (gconstpointer data, const gchar *data2, guint len);

typedef gconstpointer (*JingleTransportCheck) (JingleContent *cn, GError **err);
typedef void (*JingleTransportHandle) (JingleAction action, gconstpointer data, LmMessageNode *node);
typedef gboolean (*JingleTransportCmp) (gconstpointer data1, gconstpointer data2);

typedef struct {
  /* check if the description of a JingleContent is correct */
  JingleAppCheck check;

  /* If we got a LM with the good xmlns */
  JingleAppHandle handle;
  
  JingleAppHandleData handle_data;

} JingleAppFuncs;

typedef struct {
  /* check if the transport of a JingleContent is correct */
  JingleTransportCheck  check;

  /* */
  JingleTransportHandle handle;
  
  JingleTransportCmp cmp;
  
} JingleTransportFuncs;


void jingle_register_app(const gchar *xmlns, JingleAppFuncs *funcs,
                         JingleTransportType type);
void jingle_register_transport(const gchar *xmlns,
                               JingleTransportFuncs *funcs,
                               JingleTransportType type,
                               JingleTransportPriority prio);
JingleAppFuncs *jingle_get_appfuncs(const gchar *xmlns);
JingleTransportFuncs *jingle_get_transportfuncs(const gchar *xmlns);
void jingle_unregister_app(const gchar *xmlns);
void jingle_unregister_transport(const gchar *xmlns);

#endif
