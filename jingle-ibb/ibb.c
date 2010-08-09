/*
 * filetransfer.c
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

#include <mcabber/modules.h>
#include <mcabber/utils.h>
#include <mcabber/xmpp_helper.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>

#include "ibb.h"

static LmMessageHandler* jingle_ibb_handler = NULL;

gconstpointer jingle_ibb_check(JingleContent *cn, GError **err);
gboolean jingle_ibb_cmp(gconstpointer data1, gconstpointer data2);
void jingle_ibb_tomessage(gconstpointer data, LmMessageNode *node);
const gchar* jingle_ibb_xmlns(void);
gconstpointer jingle_ibb_new(void);

static void jingle_ibb_init(void);
static void jingle_ibb_uninit(void);


const gchar *deps[] = { "jingle", NULL };

static JingleTransportFuncs funcs = {
  jingle_ibb_xmlns,
  jingle_ibb_check,
  jingle_ibb_tomessage,
  jingle_ibb_cmp,
  jingle_ibb_new
};

module_info_t  info_jingle_inbandbytestream = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle In Band Bytestream (XEP-0261)\n",
  .requires        = deps,
  .init            = jingle_ibb_init,
  .uninit          = jingle_ibb_uninit,
  .next            = NULL,
};


const gchar* jingle_ibb_xmlns(void)
{
  return NS_JINGLE_TRANSPORT_IBB;
}

gconstpointer jingle_ibb_check(JingleContent *cn, GError **err)
{
  JingleIBB *ibb = NULL;
  LmMessageNode *node = cn->transport;
  const gchar *blocksize;

  blocksize  = lm_message_node_get_attribute(node, "block-size");
  ibb->sid = lm_message_node_get_attribute(node, "sid");
   
  if (!ibb->sid || !blocksize) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the transport element is missing");
    g_free(ibb);
    return NULL;
  }
  
  ibb->blocksize = g_ascii_strtoll(blocksize, NULL, 10);

  // the size attribute is a xs:short an therefore can be negative.
  if (ibb->blocksize < 0 || ibb->blocksize > IBB_BLOCK_SIZE_MAX) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "block-size is negative");
    g_free(ibb);
    return NULL;
  }

  return (gconstpointer) ibb;
}

LmHandlerResult jingle_ibb_handle_iq(LmMessageHandler *handler,
                                 LmConnection *connection, LmMessage *message,
                                 gpointer user_data)
{
  const gchar *data64;
  JingleIBB *ibb = g_new0(JingleIBB, 1);
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
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;

  jingle_ack_iq(message);
  
  ibb->sid = lm_message_node_get_attribute(dnode, "sid");
  ibb->seq = lm_message_node_get_attribute(dnode, "seq");
  
  data64 = lm_message_node_get_value(dnode);
  
  data = g_base64_decode(data64, &len);
  
  handle_trans_data(NS_JINGLE_TRANSPORT_IBB, ibb, (const gchar *)data, (guint)len);
  
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}


gboolean jingle_ibb_cmp(gconstpointer data1, gconstpointer data2)
{
  const JingleIBB *ibb1 = data1, *ibb2 = data2;
  if(g_strcmp0(ibb1->sid, ibb2->sid))
    return FALSE;
  return TRUE;
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

gconstpointer jingle_ibb_new(void)
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
  if(!g_strcmp0(ibb1->sid, ibb2->sid) && ibb1->seq == ibb2->seq + 1) {
    // TODO: change the seq in the session
    return 0;
  }
  return 1;
}

void jingle_ibb_tomessage(gconstpointer data, LmMessageNode *node)
{
  JingleIBB *jibb = (JingleIBB*) data;
  gchar *bsize = g_strdup_printf("%i", jibb->blocksize);
  if (lm_message_node_get_child(node, "transport") != NULL)
    return;

  LmMessageNode *node2 = lm_message_node_add_child(node, "transport", NULL);
  lm_message_node_set_attributes(node2, "xmlns", NS_JINGLE_TRANSPORT_IBB,
                                 "sid", jibb->sid,
                                 "block-size", bsize);
  g_free(bsize);
}

static void jingle_ibb_init(void)
{
  jingle_ibb_handler = lm_message_handler_new(jingle_ibb_handle_iq, NULL, NULL);
  jingle_register_transport(NS_JINGLE_TRANSPORT_IBB, &funcs,
                            JINGLE_TRANSPORT_STREAMING,
                            JINGLE_TRANSPORT_LOW);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_IBB);
}

static void jingle_ibb_uninit(void)
{
  lm_message_handler_invalidate(jingle_ibb_handler);
  lm_message_handler_unref(jingle_ibb_handler);
  xmpp_del_feature(NS_JINGLE_TRANSPORT_IBB);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_IBB);
}
