/**
 * @file register.h
 * @brief register.c header file
 * @author Nicolas Cornu
 */

#ifndef __JINGLE_REGISTER_H__
#define __JINGLE_REGISTER_H__

#include <jingle/jingle.h>

#define NS_JINGLE_APP_PREFIX       "urn:xmpp:jingle:apps:"
#define NS_JINGLE_TRANSPORT_PREFIX "urn:xmpp:jingle:transports:"


/**
 * @brief A transport can be either of type "streaming" or "datagram"
 */
typedef enum {
  /**
   * A datagram transport has one or more components with which to exchange
   * packets with UDP-like behavior. Packets might be of arbitrary length,
   * might be received out of order, and might not be received at all
   * (i.e., the transport is lossy).
   */
  JINGLE_TRANSPORT_STREAMING,
  
  /**
   * A streaming transport has one or more components with which to exchange
   * bidirectional bytestreams with TCP-like behavior. Bytes are received
   * reliably and in order, and applications MUST NOT rely on a stream being
   * chunked in any specific way.
   */
  JINGLE_TRANSPORT_DATAGRAM
} JingleTransportType;

/**
 * @brief A way for transport modules to rank themselves.
 * 
 * We need to rank transports to determine which one to choose.
 * With this system, In-Band Bytestreams could have a low priority,
 * SOCKS5 Bytestreams a normal priority.\n
 * 0 will always be an invalid priority.
 */
typedef enum {
  JINGLE_TRANSPORT_PRIO_LOW = 1,
  JINGLE_TRANSPORT_PRIO_NORMAL,
  JINGLE_TRANSPORT_PRIO_HIGH
} JingleTransportPriority;

typedef gconstpointer (*JingleAppCheck) (JingleContent *cn, GError **err);
typedef gboolean (*JingleAppHandle) (JingleAction action, gconstpointer data, LmMessageNode *node);
typedef void (*JingleAppToMessage) (gconstpointer data, LmMessageNode *node);
typedef gboolean (*JingleAppHandleData) (gconstpointer data, const gchar *data2, guint len);
typedef void (*JingleAppStart) (session_content *sc);
typedef void (*JingleAppSend) (session_content *sc);
typedef void (*JingleAppStop) (gconstpointer data);
typedef gchar* (*JingleAppInfo) (gconstpointer data);

typedef gconstpointer (*JingleTransportCheck) (JingleContent *cn, GError **err);
typedef void (*JingleTransportToMessage) (gconstpointer data, LmMessageNode *node);
typedef gconstpointer (*JingleTransportNew) (void);
typedef void (*JingleTransportSend) (session_content *sc, gconstpointer data, gchar *buf, gsize size);
typedef void (*JingleTransportInit) (session_content *sc, gconstpointer data);
typedef void (*JingleTransportEnd) (session_content *sc, gconstpointer data);
typedef gchar* (*JingleTransportInfo) (gconstpointer data);

/**
 * @brief Struct containing functions provided by an app module.
 */
typedef struct {
  /**
   * @brief Check if the description node of a JingleContent is correct
   */
  JingleAppCheck check;

  /**
   * @brief Handle an incoming jingle message (session-info, description-info...)
   * 
   * If the function could not handle the incoming data, the caller should
   * reply to the incoming message with an error iq.
   */
  JingleAppHandle handle;

  /**
   * @brief Insert data from the gconstpointer to the node given as an argument
   */
  JingleAppToMessage tomessage;

  /**
   * @brief Handle incoming data
   */
  JingleAppHandleData handle_data;

  JingleAppStart start;

  JingleAppSend send;

  JingleAppStop stop;

  JingleAppInfo info;

} JingleAppFuncs;

typedef struct {
  /**
   * @brief Check if the transport node of a JingleContent is correct
   */
  JingleTransportCheck check;

  /**
   * @brief Insert data from the gconstpointer to the node given as an argument
   */
  JingleTransportToMessage tomessage;

  JingleTransportNew new;

  JingleTransportSend send;

  JingleTransportInit init;

  JingleTransportEnd end;

  JingleTransportInfo info;
  
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
const gchar *jingle_transport_for_app(const gchar *appxmlns, GSList **forbid);

#endif
