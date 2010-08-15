#ifndef __JINGLEFT_H__
#define __JINGLEFT_H__ 1

#define NS_JINGLE_APP_FT      "urn:xmpp:jingle:apps:file-transfer:1"
#define NS_JINGLE_APP_FT_INFO "urn:xmpp:jingle:apps:file-transfer:info:1"
#define NS_SI_FT              "http://jabber.org/protocol/si/profile/file-transfer"
#define JINGLE_FT_SIZE_READ 2048

typedef enum {
  JINGLE_FT_OFFER,
  JINGLE_FT_REQUEST,
} JingleFTType;

typedef struct {
  /* the last modification of the file, optional */
  time_t date;

  /* MD5 hash of the file, optional */
  gchar *hash;

  /* the name of the file that the sender wishes to send */
  gchar *name;

  /* the size, in bytes, of the data to be sent */
  guint64 size;

  /* Data already send/receive*/
  guint64 transmit;
  
  /* descriptor to the output file */
  GIOChannel *outfile;
  
  /* Is it an offer or a request ? */
  JingleFTType type;
  
  /* A little description of the transfer */
  gchar *desc;
  
  /* Where we compute the hash */
  GChecksum *md5;
  
} JingleFT;

#endif
