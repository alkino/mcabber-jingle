#ifndef __JINGLE_REGISTER_H__
#define __JINGLE_REGISTER_H__

#include "jingle.h"


#define NS_JINGLE_APP_PREFIX       "urn:xmpp:jingle:app:"
#define NS_JINGLE_TRANSPORT_PREFIX "urn:xmpp:jingle:transport:"


typedef void (*JingleAppHandler) (JingleNode *jn, JingleContentNode *cn, gpointer *data);
typedef void (*JingleTransportHandler) (JingleNode *jn, JingleContentNode *cn, gpointer *data);


gboolean jingle_register_app(const gchar *xmlns,
                             JingleAppHandler func,
                             gpointer data);
gboolean jingle_register_transport(const gchar *xmlns,
                                   JingleTransportHandler func,
                                   gpointer data);

#endif
