/*
 * ibb.c
 *
 * Copyrigth (C) 2010 Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include <mcabber/xmpp.h>
#include <mcabber/modules.h>
#include <mcabber/utils.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/logprint.h>
#include <mcabber/hooks.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>
#include <jingle/sessions.h>

#include "ibb.h"

static LmMessageHandler* jingle_ibb_handler = NULL;

static gconstpointer newfrommessage(JingleContent *cn, GError **err);
static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err);
static void tomessage(gconstpointer data, LmMessageNode *node);
static gconstpointer new(void);
static void send(session_content *sc, gconstpointer data, gchar *buf, gsize size);
static void init(session_content *sc);
static void end(session_content *sc, gconstpointer data);
static gchar *info(gconstpointer data);

static void _send_internal(session_content *sc, const gchar *to, gchar *buf,
                           gsize size, const gchar *sid, gint64 *seq);
                           
static void jingle_ibb_init(void);
static void jingle_ibb_uninit(void);


static guint connect_hid = 0;
static guint disconn_hid = 0;

const gchar *deps[] = { "jingle", NULL };

static JingleTransportFuncs funcs = {
  .newfrommessage = newfrommessage,
  .handle         = handle,
  .tomessage      = tomessage,
  .new            = new,
  .send           = send,
  .init           = init,
  .end            = end,
  .info           = info
};

module_info_t  info_jingle_ibb = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle In Band Bytestream (XEP-0261)\n",
  .requires        = deps,
  .init            = jingle_ibb_init,
  .uninit          = jingle_ibb_uninit,
  .next            = NULL,
};

/* Hash-Table of all emited JingleIBB struct */
static GHashTable *JingleIBBs = NULL;

static gconstpointer newfrommessage(JingleContent *cn, GError **err)
{
  JingleIBB *ibb = NULL;
  LmMessageNode *node = cn->transport;
  const gchar *blocksize;

  ibb = g_new0(JingleIBB, 1);
  
  blocksize  = lm_message_node_get_attribute(node, "block-size");
  ibb->sid = g_strdup(lm_message_node_get_attribute(node, "sid"));
   
  if (!ibb->sid || !blocksize) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the transport element is missing");
    g_free(ibb);
    return NULL;
  }
  
  ibb->blocksize = g_ascii_strtoll(blocksize, NULL, 10);

  // If block size is too big, we change it
  if (ibb->blocksize > IBB_BLOCK_SIZE_MAX)
    ibb->blocksize = IBB_BLOCK_SIZE_MAX;
  
  // the blocksize attribute is a xs:short an therefore can be negative.
  if (ibb->blocksize < 0) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "block-size is negative");
    g_free(ibb->sid);
    g_free(ibb);
    return NULL;
  }

  g_hash_table_insert(JingleIBBs, ibb->sid, ibb);
  return (gconstpointer) ibb;
}

static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err)
{
  JingleIBB *jibb = (JingleIBB *)data;
  const gchar *blocksizestr;
  guint64 blocksize;

  if (action == JINGLE_SESSION_ACCEPT) {
    const gchar *xmlns = lm_message_node_get_attribute(node, "xmlns");
    if (!g_strcmp0(xmlns, NS_JINGLE_TRANSPORT_IBB)) {
      /* If the responder wishes to use a smaller block-size, the responder can
       * specify that in the session-accept by returning a different value for
       * the 'block-size' attribute. */
      blocksizestr = lm_message_node_get_attribute(node, "block-size");
      blocksize = g_ascii_strtoll(blocksizestr, NULL, 10);
      jibb->blocksize = (blocksize < jibb->blocksize) ? blocksize : jibb->blocksize; 
      return JINGLE_STATUS_HANDLED;
	}
  }
  return JINGLE_STATUS_NOT_HANDLED;
}

LmHandlerResult jingle_ibb_handle_iq(LmMessageHandler *handler,
                                 LmConnection *connection, LmMessage *message,
                                 gpointer user_data)
{
  const gchar *data64;
  JingleIBB *jibb = g_new0(JingleIBB, 1), *jibb2;
  gsize len;
  guchar *data;
  
  LmMessageSubType iqtype = lm_message_get_sub_type(message);
  if (iqtype != LM_MESSAGE_SUB_TYPE_SET)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  LmMessageNode *root = lm_message_get_node(message);
  LmMessageNode *dnode = lm_message_node_get_child(root, "data");

  if (!dnode) // no <data> element found
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  if (g_strcmp0(lm_message_node_get_attribute(dnode, "xmlns"),
                NS_TRANSPORT_IBB))
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  jibb2 = g_hash_table_lookup(JingleIBBs,
                              lm_message_node_get_attribute(dnode, "sid"));
  if (jibb2 == NULL)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  jingle_ack_iq(message);
  
  jibb->sid = g_strdup(lm_message_node_get_attribute(dnode, "sid"));
  jibb->seq = g_ascii_strtoll(lm_message_node_get_attribute(dnode, "seq"), NULL, 10);
  
  data64 = lm_message_node_get_value(dnode);
  
  data = g_base64_decode(data64, &len);

  handle_trans_data(jibb2, (const gchar *)data, (guint)len);
  
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static gchar *new_ibb_sid(void)
{
  gchar *sid;
  gchar car[] = "azertyuiopqsdfghjklmwxcvbn1234567890AZERTYUIOPQSDFGHJKLMWXCVBN";
  int i;
  sid = g_new0(gchar, 7);
  for (i = 0; i < 6; i++)
    sid[i] = car[g_random_int_range(0, sizeof(car)/sizeof(car[0]))];

  sid[6] = '\0';
  
  return sid;
}

static gconstpointer new(void)
{
  JingleIBB *ibb = g_new0(JingleIBB, 1);
  ibb->blocksize = IBB_BLOCK_SIZE_MAX;
  ibb->sid = new_ibb_sid();
  ibb->seq = 0;
  
  return ibb;
}

int jingle_ibb_check_session(gconstpointer data, gconstpointer session)
{
  const JingleIBB *ibb1 = data, *ibb2 = session;
  if(!g_strcmp0(ibb1->sid, ibb2->sid) && ibb1->seq == ibb2->seq + 1)
    return 0;
  return 1;
}

static void tomessage(gconstpointer data, LmMessageNode *node)
{
  JingleIBB *jibb = (JingleIBB*) data;
  gchar *bsize = g_strdup_printf("%i", jibb->blocksize);

  if (lm_message_node_get_child(node, "transport") != NULL)
    return;

  LmMessageNode *node2 = lm_message_node_add_child(node, "transport", NULL);

  lm_message_node_set_attributes(node2, "xmlns", NS_JINGLE_TRANSPORT_IBB,
                                 "sid", jibb->sid,
                                 "block-size", bsize,
                                 NULL);
  g_free(bsize);
}

static void jingle_ibb_handle_ack_iq_send(JingleAckType type, LmMessage *mess,
                                          gpointer data)
{
  // TODO: check the sub type (error ??)
  session_content *sc = (session_content *)data;
  
  JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
  
  // If there is no more session, maybe it's finish
  if (sess == NULL)
    return;
  
  SessionContent *sc2 = session_find_sessioncontent(sess, sc->name);

  JingleIBB *jibb = (JingleIBB *)sc2->transport;

  // We look if there is enough data staying
  if (jibb->dataleft > jibb->blocksize) {
    gchar *buf = g_memdup(jibb->buf, jibb->blocksize);
    jibb->dataleft-=jibb->blocksize;
    g_memmove(jibb->buf, jibb->buf+jibb->blocksize, jibb->dataleft);
    _send_internal(sc, sess->to, buf, jibb->blocksize, jibb->sid, &jibb->seq);
    g_free(buf);
  } else { // ask for more data
    handle_trans_next(sc);
  }
}

static void _send_internal(session_content *sc, const gchar *to, gchar *buf,
                           gsize size, const gchar *sid, gint64 *seq)
{
  JingleAckHandle *ackhandle;
  LmMessage *r = lm_message_new_with_sub_type(to, LM_MESSAGE_TYPE_IQ,
                                              LM_MESSAGE_SUB_TYPE_SET);
  LmMessageNode *node = lm_message_get_node(r);

  gchar *base64 = g_base64_encode((const guchar *)buf, size);
  gchar *strseq = g_strdup_printf("%" G_GINT64_FORMAT, *seq);

  node = lm_message_node_add_child(node, "data", NULL);
  lm_message_node_set_attributes(node, "xmlns", NS_TRANSPORT_IBB,
                                 "sid", sid,
                                 "seq", strseq,
                                 NULL);
  lm_message_node_set_value(node, base64);

  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = jingle_ibb_handle_ack_iq_send;
  ackhandle->user_data = (gpointer)sc;

  lm_connection_send_with_reply(lconnection, r,
                                jingle_new_ack_handler(ackhandle), NULL);
  lm_message_unref(r);

  // The next packet will be seq++
  ++(*seq);

  g_free(base64);
  g_free(strseq);
}

static void send(session_content *sc, gconstpointer data, gchar *buf,
                     gsize size)
{
  JingleIBB *jibb = (JingleIBB*)data;
  
  if (jibb->size_buf >= size + jibb->dataleft) {
    g_memmove(jibb->buf + jibb->dataleft, buf, size);
    jibb->dataleft+=size;
  } else {
    jibb->size_buf = size + jibb->dataleft;
    jibb->buf = g_realloc(jibb->buf, jibb->size_buf);
    g_memmove(jibb->buf + jibb->dataleft, buf, size);
    jibb->dataleft = jibb->size_buf;
  }

  // We need more data
  if (jibb->dataleft < jibb->blocksize) {
    handle_trans_next(sc);
    return;
  }

  // It's enough
  {
    JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
    
    gchar *buffer = g_memdup(jibb->buf, jibb->blocksize);
  
    jibb->dataleft-=jibb->blocksize;
  
    g_memmove(jibb->buf, jibb->buf+jibb->blocksize, jibb->dataleft);
  
    _send_internal(sc, sess->to, buffer, jibb->blocksize, jibb->sid,
                   &jibb->seq);
  
    g_free(buffer);
  }
}

static void init(session_content *sc)
{
  return;
}

static void end(session_content *sc, gconstpointer data)
{
  JingleIBB *jibb = (JingleIBB*)data;
  JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
  
  if (jibb->dataleft > 0) {
    _send_internal(sc, sess->to, jibb->buf, jibb->dataleft, jibb->sid,
                   &jibb->seq);
  }
  
  g_free(jibb->buf);
}

static void jingle_ibb_unregister_lm_handlers(void)
{
  if (lconnection) {
    lm_connection_unregister_message_handler(lconnection, jingle_ibb_handler,
        LM_MESSAGE_TYPE_IQ);
  }
}

static void jingle_ibb_register_lm_handlers(void)
{
  jingle_ibb_unregister_lm_handlers();
  if (lconnection) {
    lm_connection_register_message_handler(lconnection, jingle_ibb_handler,
        LM_MESSAGE_TYPE_IQ,
        LM_HANDLER_PRIORITY_FIRST);
  }
}

static guint jingle_ibb_connect_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore)
{
  jingle_ibb_register_lm_handlers();
  return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static guint jingle_ibb_disconn_hh(const gchar *hname, hk_arg_t *args,
                               gpointer ignore)
{
  jingle_ibb_unregister_lm_handlers();
  return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static gchar *info(gconstpointer data)
{
  JingleIBB *jibb = (JingleIBB *)data;
  gchar *info = g_strdup_printf("IBB %i", jibb->blocksize);
  return info;
}

static void free_ibb(gpointer data)
{
  JingleIBB *jibb = (JingleIBB *)data;
  g_free(jibb);
}

static void jingle_ibb_init(void)
{
  jingle_ibb_handler = lm_message_handler_new(jingle_ibb_handle_iq, NULL, NULL);
  
  connect_hid = hk_add_handler(jingle_ibb_connect_hh, HOOK_POST_CONNECT,
      G_PRIORITY_DEFAULT_IDLE, NULL);
  disconn_hid = hk_add_handler(jingle_ibb_disconn_hh, HOOK_PRE_DISCONNECT,
      G_PRIORITY_DEFAULT_IDLE, NULL);
  jingle_ibb_register_lm_handlers();
  
  jingle_register_transport(NS_JINGLE_TRANSPORT_IBB, &funcs,
                            JINGLE_TRANSPORT_STREAMING,
                            JINGLE_TRANSPORT_PRIO_LOW);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_IBB);
  JingleIBBs = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_ibb);
}

static void jingle_ibb_uninit(void)
{
  lm_message_handler_invalidate(jingle_ibb_handler);
  lm_message_handler_unref(jingle_ibb_handler);
  xmpp_del_feature(NS_JINGLE_TRANSPORT_IBB);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_IBB);
}
