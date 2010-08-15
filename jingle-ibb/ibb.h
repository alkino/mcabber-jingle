#ifndef __JINGLEIBB_H__
#define __JINGLEIBB_H__ 1

#define NS_JINGLE_TRANSPORT_IBB "urn:xmpp:jingle:transports:ibb:0"
#define NS_TRANSPORT_IBB "http://jabber.org/protocol/ibb"

#define IBB_BLOCK_SIZE_MAX 8192

typedef struct {
  /* Size of the blocks */
  guint blocksize;

  /* The identifiant of the transfer */
  gchar *sid;

  gchar *buf;
  
  gint size_buf;
  
  gint dataleft;
  
  gint64 seq;
  
} JingleIBB;

#endif
