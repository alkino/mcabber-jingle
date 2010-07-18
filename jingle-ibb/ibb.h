#ifndef __JINGLEIBB_H__
#define __JINGLEIBB_H__ 1

#define NS_JINGLE_TRANSPORT_IBB "urn:xmpp:jingle:transports:ibb:1"
#define NS_TRANSPORT_IBB "http://jabber.org/protocol/ibb"

typedef struct {
  /* Size of the blocks */
  int blocksize;

  /* The identifiant of the transfer */
  const gchar *sid;
  
  guchar *data;

  const gchar *seq;
  
} JingleIBB;

#endif
