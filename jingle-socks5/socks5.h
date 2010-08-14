#ifndef __JINGLESOCKS5_H__
#define __JINGLESOCKS5_H__ 1

#define NS_JINGLE_TRANSPORT_SOCKS5 "urn:xmpp:jingle:transports:s5b:1"

typedef enum {
  JINGLE_S5B_ASSISTED,
  JINGLE_S5B_DIRECT,
  JINGLE_S5B_PROXY,
  JINGLE_S5B_TUNNEL
} JingleS5BType;

typedef enum {
  JINGLE_S5B_TCP,
  JINGLE_S5B_UDP
} JingleS5BModes;

typedef struct {
  JingleS5BModes mode;
  
  const gchar *sid;  
   
  GSList *candidates;
} JingleSocks5;
 
typedef struct {
  const gchar *cid;
  
  const gchar *host;
  
  const gchar *jid;
  
  guint16 port;
  
  guint64 priority;
  
  JingleS5BType type;
  
} JingleS5BCandidate;
#endif
