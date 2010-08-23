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

/**
 * @brief Type returned by JingleAppFuncs and JingleTransportFuncs ->handle
 */
typedef enum {
  JINGLE_STATUS_HANDLED,
  JINGLE_STATUS_NOT_HANDLED,
  JINGLE_STATUS_HANDLE_ERROR
} JingleHandleStatus;

typedef gconstpointer (*JingleAppNewFromMsg) (JingleContent *cn, GError **err);
typedef JingleHandleStatus (*JingleAppHandle) (JingleAction action, gconstpointer data, LmMessageNode *node, GError **err);
typedef void (*JingleAppToMessage) (gconstpointer data, LmMessageNode *node);
typedef gboolean (*JingleAppHandleData) (gconstpointer data, const gchar *data2, guint len);
typedef void (*JingleAppStart) (session_content *sc);
typedef void (*JingleAppSend) (session_content *sc);
typedef void (*JingleAppStop) (gconstpointer data);
typedef gchar* (*JingleAppInfo) (gconstpointer data);

typedef gconstpointer (*JingleTransportNewFromMsg) (JingleContent *cn, GError **err);
typedef JingleHandleStatus (*JingleTransportHandle) (JingleAction action, gconstpointer data, LmMessageNode *node, GError **err);
typedef void (*JingleTransportToMessage) (gconstpointer data, LmMessageNode *node);
typedef gconstpointer (*JingleTransportNew) (void);
typedef void (*JingleTransportSend) (session_content *sc, gconstpointer data, gchar *buf, gsize size);
typedef void (*JingleTransportInit) (session_content *sc);
typedef void (*JingleTransportEnd) (session_content *sc, gconstpointer data);
typedef gchar* (*JingleTransportInfo) (gconstpointer data);

/**
 * @brief Struct containing functions provided by an app module.
 */
typedef struct {
  /**
   * @brief Check a description node, store its content in an internal
   *        struct, then return it as a gconstpointer
   * 
   * This function will only called when receiving a session-initiate.
   * It should store the content of the <description> node into an
   * internal struct and return this struct as a gconstpointer.
   */
  JingleAppNewFromMsg newfrommessage;

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
   * @brief Handle incoming Jingle IQs.
   * 
   * e.g.: decription-info, content-info... etc
   * The first argument is the IQ type (a JingleAction). If the function
   * doesn't want to handle the IQ, it simply returns JINGLE_NOT_HANDLED,
   * if it has handled the IQ, it returns JINGLE_HANDLED, if it has handled
   * the IQ and an error happened, it returns JINGLE_HANDLE_ERROR.
   */
  JingleAppHandleData handle_data;

  JingleAppStart start;

  JingleAppSend send;

  JingleAppStop stop;

  JingleAppInfo info;

} JingleAppFuncs;

typedef struct {
  /**
   * @brief Check a transport node, store its content in an internal
   *        struct, then return it as a gconstpointer
   * 
   * It basically does the same thins as a JingleAppNewFroMsg function,
   * but for a <transport> node.
   */
  JingleTransportNewFromMsg newfrommessage;

  JingleTransportHandle handle;

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
