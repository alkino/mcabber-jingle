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
#include <glib/gstdio.h>
#include <string.h>

#include <mcabber/modules.h>
#include <mcabber/utils.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/settings.h>
#include <mcabber/logprint.h>
#include <mcabber/compl.h>
#include <mcabber/commands.h>
#include <mcabber/roster.h>
#include <mcabber/utils.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>
#include <jingle/sessions.h>
#include <jingle/send.h>

#include "filetransfer.h"


gconstpointer jingle_ft_check(JingleContent *cn, GError **err);
void jingle_ft_tomessage(gconstpointer data, LmMessageNode *node);
gboolean jingle_ft_handle_data(gconstpointer data, const gchar *data2, guint len);
void jingle_ft_start(session_content *sc, gsize size);
void jingle_ft_send(session_content *sc, gsize size);
static gboolean is_md5_hash(const gchar *hash);
static void jingle_ft_init(void);
static void jingle_ft_uninit(void);

const gchar *deps[] = { "jingle", NULL };

static JingleAppFuncs funcs = {
  jingle_ft_check,
  jingle_ft_tomessage,
  jingle_ft_handle_data,
  jingle_ft_start,
  jingle_ft_send
};

module_info_t info_jingle_filetransfer = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle File Transfer (XEP-0234)\n",
  .requires        = deps,
  .init            = jingle_ft_init,
  .uninit          = jingle_ft_uninit,
  .next            = NULL,
};


gconstpointer jingle_ft_check(JingleContent *cn, GError **err)
{
  JingleFT *ft = NULL;
  LmMessageNode *node;
  gint64 tmpsize;
  const gchar *datestr, *sizestr;

  node = lm_message_node_get_child(cn->description, "offer");
 
  if (!node) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "the offer element is missing");
    return NULL;
  }

  node = lm_message_node_get_child(node, "file");
  if (!node) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "the file element is missing");
    return NULL;
  }

  if (g_strcmp0(lm_message_node_get_attribute(node, "xmlns"), NS_SI_FT)) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "the file transfer offer has an invalid/unsupported namespace");
    return NULL;
  }

  ft = g_new0(JingleFT, 1);
  datestr  = lm_message_node_get_attribute(node, "date");
  ft->hash = (gchar *) lm_message_node_get_attribute(node, "hash");
  ft->name = (gchar *) lm_message_node_get_attribute(node, "name");
  sizestr  = lm_message_node_get_attribute(node, "size");

  if (!ft->name || !sizestr) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the file element is missing");
    g_free(ft);
    return NULL;
  }

  ft->date = from_iso8601(datestr, 1);
  tmpsize = g_ascii_strtoll(sizestr, NULL, 10);

  // the size attribute is a xs:integer an therefore can be negative.
  if (tmpsize < 0) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the offered file has a negative size");
    g_free(ft);
    return NULL;
  }
  ft->size = tmpsize;

  ft->name = g_path_get_basename(ft->name);
  
  if (settings_opt_get("jingle_ft_dir") != NULL)
    ft->name = g_build_filename(settings_opt_get("jingle_ft_dir"), ft->name, NULL);
  else
    ft->name = g_build_filename("/tmp", ft->name, NULL);

  if (!g_strcmp0(ft->name, ".")) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the offered file has an invalid filename");
    g_free(ft->name);
    g_free(ft);
    return NULL;
  }

  // check if the md5 hash is valid ([a-fA-F0-9){32})
  if (ft->hash != NULL && (strlen(ft->hash) != 32 || !is_md5_hash(ft->hash))) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the offered file has an invalid filename");
    g_free(ft->name);
    g_free(ft);
    return NULL;
  }
  ft->hash = g_strndup(ft->hash, 32);

  return (gconstpointer) ft;
}

static gboolean is_md5_hash(const gchar *hash)
{
  int i = 0;
  for (i = 0; i < 32 && hash[i]; i++)
    if (!g_ascii_isxdigit(hash[i])) break;

  if (i == 32)
    return TRUE;
  else
    return FALSE;
}

gboolean jingle_ft_handle_data(gconstpointer jingleft, const gchar *data, guint len)
{
  JingleFT *jft = (JingleFT *) jingleft;
  GError *err = NULL;
  GIOStatus status;
  gsize bytes_written = 0;

  // TODO: check if the file already exist or if it was created
  // during the call to jingle_ft_check and handle_data
  if (jft->outfile == NULL) {
    jft->outfile = g_io_channel_new_file(jft->name, "w", &err);
    if (jft->outfile == NULL || err != NULL) {
      // propagate the GError ?
      return FALSE;
	}
	g_io_channel_set_encoding(jft->outfile, NULL, NULL);
  }

  status = g_io_channel_write_chars(jft->outfile, data, (gssize) len,
                                    &bytes_written, &err);
  g_io_channel_flush(jft->outfile, NULL);
  if (status != G_IO_STATUS_NORMAL || err != NULL) {
    return FALSE;
  }
  if (bytes_written != len) {
    // not supposed to happen if status is normal, unless outfile is non-blocking
    return FALSE;
  }
  return TRUE;
}

static void do_sendfile(char *arg)
{
  char **args = split_arg(arg, 1, 0);
  gchar *filename;
  
  if (!args[0]) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a name!");
    return;
  }
  
  filename = expand_filename(args[0]); // expand ~ to HOME
  
  if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: File doesn't exist!");
    return;
  }
  
  scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Trying to send %s",
               args[0]);

  {
    JingleSession *sess;
    gchar *sid = jingle_generate_sid();
    gchar *ressource, *recipientjid;
    const gchar *namespaces[] = {NS_JINGLE, NS_JINGLE_APP_FT, NULL};
    struct stat fileinfo;
    const gchar *myjid = g_strdup(lm_connection_get_jid(lconnection));
    JingleFT *jft = g_new0(JingleFT, 1);

    if (CURRENT_JID == NULL) { // CURRENT_JID = the jid of the user which has focus
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Please, choose a valid JID in the roster");
      return;
    }
    ressource = jingle_find_compatible_res(CURRENT_JID, namespaces);
    if (ressource == NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Cannot send file, because there is no ressource available");
      return;
    }
    
    recipientjid = g_strdup_printf("%s/%s", CURRENT_JID, ressource);

    sess = session_new(sid, myjid, recipientjid, JINGLE_SESSION_OUTGOING);
    session_add_content(sess, "file", JINGLE_SESSION_STATE_PENDING);

    if (g_stat(filename, &fileinfo) != 0) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: unable to stat %s", args[1]);
      return;
    }
    jft->desc = g_strdup(args[0]);
    jft->type = JINGLE_FT_OFFER;
    jft->name = g_path_get_basename(filename);
    jft->date = fileinfo.st_mtime;
    jft->size = fileinfo.st_size;
    jft->outfile = g_io_channel_new_file (filename, "r", NULL);
    if (jft->outfile == NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Cannot open file %s", args[1]);
      return;
    }
    
    g_io_channel_set_encoding(jft->outfile, NULL, NULL);
    
    session_add_app(sess, "file", NS_JINGLE_APP_FT, jft);

    jingle_handle_app(sess, "file", NS_JINGLE_APP_FT, jft, recipientjid);

    g_free(ressource);
    g_free(sid);
    //g_io_channel_unref(jft->outfile);
    //g_io_channel_shutdown(jft->outfile, TRUE, NULL);
  }

  free_arg_lst(args);
}

void jingle_ft_tomessage(gconstpointer data, LmMessageNode *node)
{
  JingleFT *jft = (JingleFT*) data;
  gchar *size = NULL;
  
  if (lm_message_node_get_child(node, "description") != NULL)
    return;

  LmMessageNode *node2 = lm_message_node_add_child(node, "description", NULL);
  lm_message_node_set_attribute(node2, "xmlns", NS_JINGLE_APP_FT);
  if (jft->type == JINGLE_FT_OFFER)
    node2 = lm_message_node_add_child(node2, "offer", NULL);
  else
    node2 = lm_message_node_add_child(node2, "request", NULL);

  node2 = lm_message_node_add_child(node2, "file", NULL);

  size = g_strdup_printf("%" G_GUINT64_FORMAT, jft->size);
  
  lm_message_node_set_attributes(node2, "xmlns", NS_SI_FT, "name", jft->name,
                                 "size", size, NULL);
  g_free(size);
  
  if (jft->hash != NULL)
    lm_message_node_set_attribute(node2, "hash", jft->hash);

  if (jft->desc != NULL)
    lm_message_node_add_child(node2, "desc", jft->desc);

  //if (jft->data != 0)
}

void jingle_ft_send_hash(gchar *sid, gchar *to, gchar *hash)
{
  JingleAckHandle *ackhandle;
  
  LmMessage *r = lm_message_new_with_sub_type(to, LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
  LmMessageNode *node = lm_message_get_node(r);
  lm_message_node_add_child(node, "jingle", NULL);
  node = lm_message_node_get_child(node, "jingle");
  lm_message_node_set_attributes(node, "xmlns", NS_JINGLE, "sid", sid, "action", "session-info", NULL);
  lm_message_node_add_child(node, "hash", hash);
  node = lm_message_node_get_child(node, "hash");
  lm_message_node_set_attribute(node, "xmlns", NS_JINGLE_APP_FT_INFO);
  
  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = NULL;
  ackhandle->user_data = NULL;
  
  lm_connection_send_with_reply(lconnection, r,
                                jingle_new_ack_handler(ackhandle), NULL);
  lm_message_unref(r);
}

void jingle_ft_send(session_content *sc, gsize size)
{
  JingleFT *jft;
  gchar *buf = g_new0(gchar, size);
  gsize read;
  GIOStatus status;
  int count = 0;
  JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
  if (sess == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: error before transfer");
    // TODO: send error
    return;
  }
  
  SessionContent *sc2 = session_find_sessioncontent(sess, sc->name);
  
  jft = (JingleFT*)sc2->description;
  
  do {
    count++;
    status = g_io_channel_read_chars(jft->outfile, (gchar*)buf, size, &read, NULL);
  } while (status == G_IO_STATUS_AGAIN && count < 10);
  
  if (status == G_IO_STATUS_AGAIN) {
    // TODO: something better
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: file unavailable");
    return;
  }
  
  if (status == G_IO_STATUS_ERROR) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: an error occured");
    return;
  }
  
  if (status == G_IO_STATUS_NORMAL) {
    g_checksum_update(jft->md5, (guchar*)buf, read);
    // Call a handle in jingle who will call the trans
    handle_app_data(sc->sid, sc->from, sc->name, buf, read);
    g_free(buf);
  }
  
  if (status == G_IO_STATUS_EOF) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: transfer finish (%s)", jft->name);
    jft->hash = g_strdup(g_checksum_get_string(jft->md5));
    // Call a function to say state is ended
    session_changestate_sessioncontent(sess, sc2->name, JINGLE_SESSION_STATE_ENDED);
    // Send the hash
    jingle_ft_send_hash(sess->sid, sess->to, jft->hash);
    g_checksum_free(jft->md5);
    
    if (!session_remove_sessioncontent(sess, sc2->name)) {
      jingle_send_session_terminate(sess, "success");
      session_delete(sess);
    }
  }
}

void jingle_ft_start(session_content *sc, gsize size)
{
  JingleFT *jft;
  
  JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
  if (sess == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: error before transfer");
    return;
  }
  
  SessionContent *sc2 = session_find_sessioncontent(sess, sc->name);

  jft = (JingleFT*)sc2->description;
  
  jft->md5 = g_checksum_new(G_CHECKSUM_MD5);
  
  sc2->appfuncs->send(sc, size);
}

static void jingle_ft_init(void)
{
  jingle_register_app(NS_JINGLE_APP_FT, &funcs, JINGLE_TRANSPORT_STREAMING);
  xmpp_add_feature(NS_JINGLE_APP_FT);
  /*file_cid = compl_new_category();
  if (file_cid) {
    compl_add_category_word(sendfile_cid, "send");
    compl_add_category_word(send file_cid, "request");
  }*/
  /* Add command */
  cmd_add("sendfile", "Send a file", 0, 0, do_sendfile, NULL);
}

static void jingle_ft_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_APP_FT);
  jingle_unregister_app(NS_JINGLE_APP_FT);
  cmd_del("file");
  /*if (file_cid)
    compl_del_category(file_cid);*/
}
