#ifndef __JINGLEFT_H__
#define __JINGLEFT_H__ 1

#define NS_JINGLE_APP_FT      "urn:xmpp:jingle:apps:file-transfer:1"
#define NS_JINGLE_APP_FT_INFO "urn:xmpp:jingle:apps:file-transfer:info:1"
#define NS_SI_FT              "http://jabber.org/protocol/si/profile/file-transfer"

typedef struct {
  /* the last modification of the file, optional */
  time_t date;

  /* MD5 hash of the file, optional */
  const gchar *hash;

  /* the name of the file that the sender wishes to send */
  const gchar *name;

  /* the size, in bytes, of the data to be sent */
  gint64 size;

} JingleFT;

#endif