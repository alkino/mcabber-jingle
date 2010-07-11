#ifndef __JINGLE_GENERAL_HANDLERS_H__
#define __JINGLE_GENERAL_HANDLERS_H__ 1

typedef void (*jingle_func_ack_iq) (LmMessage*, void*);

typedef struct {
  gchar *id;
  jingle_func_ack_iq callback;
  void *udata;
} ack_iq;

gboolean evscallback_jingle(guint evcontext, const gchar *arg,
                            gpointer userdata);
LmHandlerResult jingle_handle_ack_iq (LmMessageHandler *handler,
                                      LmConnection *connection, 
                                      LmMessage *message, gpointer user_data);
void add_ack_wait(ack_iq *elem);
#endif
