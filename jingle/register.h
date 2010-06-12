#ifndef __JINGLE_REGISTER_H__
#define __JINGLE_REGISTER_H__

#include <jingle/jingle.h>


#define NS_JINGLE_APP_PREFIX       "urn:xmpp:jingle:app:"
#define NS_JINGLE_TRANSPORT_PREFIX "urn:xmpp:jingle:transport:"


typedef gconstpointer (*JingleAppCheck) (JingleContent *cn, GError **err, gpointer *data);
typedef void (*JingleAppHandle) (JingleNode *jn, JingleContent *cn, gpointer *data);
typedef gconstpointer (*JingleTransportCheck) (JingleContent *cn, GError **err, gpointer *data);
typedef void (*JingleTransportHandle) (JingleNode *jn, JingleContent *cn, gpointer *data);

typedef struct {
  /* check if the description of a JingleContent is correct */
  JingleAppCheck  check;

  /* */
  JingleAppHandle handle;

} JingleAppFuncs;

typedef struct {
  /* check if the transport of a JingleContent is correct */
  JingleAppCheck  check;

  /* */
  JingleAppHandle handle;
  
} JingleTransportFuncs;


void jingle_register_app(const gchar *xmlns,
                         JingleAppFuncs *funcs,
                         gpointer data);
void jingle_register_transport(const gchar *xmlns,
                               JingleTransportFuncs *funcs,
                               gpointer data);
JingleAppFuncs *jingle_get_appfuncs(const gchar *xmlns);
JingleTransportFuncs *jingle_get_transportfuncs(const gchar *xmlns);
void jingle_unregister_app(const gchar *xmlns);
void jingle_unregister_transport(const gchar *xmlns);

#endif
