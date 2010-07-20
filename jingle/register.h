#ifndef __JINGLE_REGISTER_H__
#define __JINGLE_REGISTER_H__

#include <jingle/jingle.h>


#define NS_JINGLE_APP_PREFIX       "urn:xmpp:jingle:apps:"
#define NS_JINGLE_TRANSPORT_PREFIX "urn:xmpp:jingle:transports:"

typedef enum {
  JINGLE_TRANS_IN_BAND,
  JINGLE_TRANS_OUT_BAND,
} JingleTransType;

typedef enum {
  JINGLE_TRANS_TCP,
  JINGLE_TRANS_UDP,
} JingleTransMethod;

typedef gconstpointer (*JingleAppCheck) (JingleContent *cn, GError **err);
typedef void (*JingleAppHandle) (JingleNode *jn, JingleContent *cn);
typedef LmMessageNode* (*JingleAppGetLM) (gconstpointer data);
typedef gboolean (*JingleAppHandleData) (gconstpointer data, const gchar *data2, guint len);

typedef gconstpointer (*JingleTransportCheck) (JingleContent *cn, GError **err);
typedef void (*JingleTransportHandle) (JingleNode *jn, JingleContent *cn);
typedef LmMessageNode* (*JingleTransportGetLM) (gconstpointer data);
typedef gboolean (*JingleTransportCmp) (gconstpointer data1, gconstpointer data2);

typedef struct {
  /* check if the description of a JingleContent is correct */
  JingleAppCheck check;

  /* Give a LM from a internal struct */
  JingleAppGetLM desc;
  
  /* If we got a LM with the good xmlns */
  JingleAppHandle handle;
  
  JingleAppHandleData handle_data;

} JingleAppFuncs;

typedef struct {
  /* check if the transport of a JingleContent is correct */
  JingleAppCheck  check;

  JingleTransportGetLM trans;

  /* */
  JingleAppHandle handle;
  
  JingleTransportCmp cmp;
  
} JingleTransportFuncs;


void jingle_register_app(const gchar *xmlns, JingleAppFuncs *funcs,
                         JingleTransMethod method);
void jingle_register_transport(const gchar *xmlns, JingleTransportFuncs *funcs,
                               JingleTransType type, JingleTransMethod method);
JingleAppFuncs *jingle_get_appfuncs(const gchar *xmlns);
JingleTransportFuncs *jingle_get_transportfuncs(const gchar *xmlns);
void jingle_unregister_app(const gchar *xmlns);
void jingle_unregister_transport(const gchar *xmlns);

#endif
