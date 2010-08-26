#ifndef __JINGLESOCKS5_H__
#define __JINGLESOCKS5_H__ 1

#include <glib.h>
#include <gio/gio.h>

#define NS_JINGLE_TRANSPORT_SOCKS5 "urn:xmpp:jingle:transports:s5b:1"


typedef enum {
  JINGLE_S5B_DIRECT,
  JINGLE_S5B_ASSISTED,
  JINGLE_S5B_TUNNEL,
  JINGLE_S5B_PROXY
} JingleS5BType;

typedef enum {
  JINGLE_S5B_TCP,
  JINGLE_S5B_UDP
} JingleS5BModes;

typedef struct {
  JingleS5BModes mode;

  const gchar *sid;

  GSocketConnection *sock;

  GSocketListener *listener;

  /**
   * This is the list of the other client's candidates.
   */
  GSList *candidates;

  /**
   * This is our list of candidates, the one we sent during a
   * session-initiate or session-accept.
   */
  GSList *ourcandidates;
} JingleS5B;
 
typedef struct {
  const gchar *cid;

  GInetAddress *host;

  const gchar *jid;

  guint16 port;

  guint64 priority;

  JingleS5BType type;

  GSocket *sock;
} S5BCandidate;

#endif
