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


static gconstpointer check(JingleContent *cn, GError **err);
static gboolean handle(JingleAction action, gconstpointer data, LmMessageNode *node);
static void tomessage(gconstpointer data, LmMessageNode *node);
static gboolean handle_data(gconstpointer data, const gchar *data2, guint len);
static void start(session_content *sc);
static void send(session_content *sc);
static void stop(gconstpointer data);

static gboolean is_md5_hash(const gchar *hash);

static void jingle_ft_init(void);
static void jingle_ft_uninit(void);

const gchar *deps[] = { "jingle", NULL };

static JingleAppFuncs funcs = {
  .check        = check,
  .handle       = handle,
  .tomessage    = tomessage,
  .handle_data  = handle_data,
  .start        = start,
  .send         = send,
  .stop         = stop
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


static gconstpointer check(JingleContent *cn, GError **err)
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
                "the offered file has an invalid hash");
    g_free(ft->name);
    g_free(ft);
    return NULL;
  }
  ft->hash = g_strndup(ft->hash, 32);

  return (gconstpointer) ft;
}

static gboolean handle(JingleAction action, gconstpointer data,
                          LmMessageNode *node)
{
  if (action == JINGLE_SESSION_INFO) {
    if (!g_strcmp0(lm_message_node_get_attribute(node, "xmlns"),
                   NS_JINGLE_APP_FT_INFO)
        && !g_strcmp0(node->name, "hash")) {
      ((JingleFT *)data)->hash = g_strdup(lm_message_node_get_value(node));
      return TRUE;
	}
	return FALSE;
  }
  return FALSE;
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

static gboolean handle_data(gconstpointer jingleft, const gchar *data, guint len)
{
  JingleFT *jft = (JingleFT *) jingleft;
  GError *err = NULL;
  GIOStatus status;
  gsize bytes_written = 0;

  if (jft->md5 == NULL) {
    jft->md5 = g_checksum_new(G_CHECKSUM_MD5);
  }
  
  g_checksum_update(jft->md5, (guchar*)data, (gsize)len);
    
  // TODO: check if the file already exist or if it was created
  // during the call to jingle_ft_check and handle_data
  if (jft->outfile == NULL) {
    jft->outfile = g_io_channel_new_file(jft->name, "w", &err);
    if (jft->outfile == NULL || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                   jft->name);
    //TODO: propagate the GError ?
      g_error_free(err);
      return FALSE;
	}
	status = g_io_channel_set_encoding(jft->outfile, NULL, &err);
	if (status != G_IO_STATUS_NORMAL || err != NULL) {
     scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                  jft->name);
     g_error_free(err);
     return FALSE;
   }
  }

  status = g_io_channel_write_chars(jft->outfile, data, (gssize) len,
                                    &bytes_written, &err);
  if (status != G_IO_STATUS_NORMAL || err != NULL) {
     scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                  jft->name);
    g_error_free(err);
    return FALSE;
  }
  status = g_io_channel_flush(jft->outfile, &err);
  if (status != G_IO_STATUS_NORMAL || err != NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                 jft->name);
    g_error_free(err);
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
  struct stat fileinfo;

  if (!args[0]) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a name!");
    return;
  }
  
  filename = expand_filename(args[0]); // expand ~ to HOME
  
  if (g_stat(filename, &fileinfo) != 0) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: unable to stat %s", args[1]);
    return;
  }
  
  if (!S_ISREG(fileinfo.st_mode) && !S_ISLNK(fileinfo.st_mode)) {
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
    const gchar *myjid = g_strdup(lm_connection_get_jid(lconnection));
    JingleFT *jft = g_new0(JingleFT, 1);
    GError *err = NULL;
    
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

    jft->desc = g_strdup(args[0]);
    jft->type = JINGLE_FT_OFFER;
    jft->name = g_path_get_basename(filename);
    jft->date = fileinfo.st_mtime;
    jft->size = fileinfo.st_size;
    jft->outfile = g_io_channel_new_file (filename, "r", &err);
    if (jft->outfile == NULL || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                   args[1]);
      g_error_free(err);
      return;
    }
    
    g_io_channel_set_encoding(jft->outfile, NULL, &err);
    if (jft->outfile == NULL || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                   args[1]);
      g_error_free(err);
      return;
    }
    
    session_add_app(sess, "file", NS_JINGLE_APP_FT, jft);

    jingle_handle_app(sess, "file", NS_JINGLE_APP_FT, jft, recipientjid);

    g_free(ressource);
    g_free(sid);
  }

  free_arg_lst(args);
}

static void tomessage(gconstpointer data, LmMessageNode *node)
{
  JingleFT *jft = (JingleFT*) data;
  gchar *size = NULL;
  gchar date[19];
  
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
  
  lm_message_node_set_attributes(node2, "xmlns", NS_SI_FT,
                                 "name", jft->name,
                                 "size", size,
                                 NULL);
  g_free(size);
  
  if (jft->hash != NULL)
    lm_message_node_set_attribute(node2, "hash", jft->hash);

  if (jft->date)
    if (!to_iso8601(date, jft->date))
      lm_message_node_set_attribute(node2, "date", date);

  if (jft->desc != NULL)
    lm_message_node_add_child(node2, "desc", jft->desc);

  //if (jft->data != 0)
}

static void send_hash(gchar *sid, gchar *to, gchar *hash)
{
  JingleAckHandle *ackhandle;
  GError *err = NULL;
  gboolean ret;
  
  LmMessage *r = lm_message_new_with_sub_type(to, LM_MESSAGE_TYPE_IQ,
                                              LM_MESSAGE_SUB_TYPE_SET);
  LmMessageNode *node = lm_message_get_node(r);
  lm_message_node_add_child(node, "jingle", NULL);
  node = lm_message_node_get_child(node, "jingle");
  lm_message_node_set_attributes(node, "xmlns", NS_JINGLE,
                                 "sid", sid,
                                 "action", "session-info",
                                 NULL);
  lm_message_node_add_child(node, "hash", hash);
  node = lm_message_node_get_child(node, "hash");
  lm_message_node_set_attribute(node, "xmlns", NS_JINGLE_APP_FT_INFO);
  
  ackhandle = g_new0(JingleAckHandle, 1);
  ackhandle->callback = NULL;
  ackhandle->user_data = NULL;
  
  ret = lm_connection_send_with_reply(lconnection, r,
                                      jingle_new_ack_handler(ackhandle), &err);
  
  // It's not really a problem, but we must tell it!
  if (ret == FALSE || err != NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: cannot send hash: %s",
                 err->message);
    g_error_free(err);
  }
  
  lm_message_unref(r);
}

static void send(session_content *sc)
{
  JingleFT *jft;
  gchar buf[JINGLE_FT_SIZE_READ];
  gsize read;
  GIOStatus status;
  int count = 0;
  GError *err = NULL;
  
  JingleSession *sess = session_find_by_sid(sc->sid, sc->from);
  if (sess == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: error before transfer");
    // We haven't LmMessage: jingle_send_iq_error(jn->message, "cancel", "item-not-found", "unknown-session");
    return;
  }
  
  SessionContent *sc2 = session_find_sessioncontent(sess, sc->name);
  
  jft = (JingleFT*)sc2->description;
  
  do {
    count++;
    status = g_io_channel_read_chars(jft->outfile, (gchar*)buf,
                                     JINGLE_FT_SIZE_READ, &read, &err);
  } while (status == G_IO_STATUS_AGAIN && count < 10);

  if (status == G_IO_STATUS_AGAIN) {
    // TODO: something better
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: file unavailable");
    return;
  }
  
  if (status == G_IO_STATUS_ERROR || err != NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s", err->message);
    g_error_free(err);
    return;
  }
  
  if (status == G_IO_STATUS_NORMAL) {
    g_checksum_update(jft->md5, (guchar*)buf, read);
    // Call a handle in jingle who will call the trans
    handle_app_data(sc->sid, sc->from, sc->name, buf, read);
  }
  
  if (status == G_IO_STATUS_EOF) {
    handle_app_data(sc->sid, sc->from, sc->name, NULL, 0);
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: transfer finish (%s)",
                 jft->name);
    jft->hash = g_strdup(g_checksum_get_string(jft->md5));
    // Call a function to say state is ended
    session_changestate_sessioncontent(sess, sc2->name, 
                                       JINGLE_SESSION_STATE_ENDED);
    // Send the hash
    send_hash(sess->sid, sess->to, jft->hash);
    g_checksum_free(jft->md5);
    
    if (!session_remove_sessioncontent(sess, sc2->name)) {
      jingle_send_session_terminate(sess, "success");
      session_delete(sess);
    }
  }
}

static void start(session_content *sc)
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
  
  scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Transfer start (%s)",
               jft->name);

  sc2->appfuncs->send(sc);
}

// When we got a session-terminate
static void stop(gconstpointer data)
{
  JingleFT *jft = (JingleFT*)data;
  GError *err = NULL;
  GIOStatus status;
  
  if (jft->outfile != NULL) {
    status = g_io_channel_flush(jft->outfile, &err);
    if (status != G_IO_STATUS_NORMAL || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s",
                   err->message);
      g_error_free(err);
    }
    g_io_channel_unref(jft->outfile);
  }

  if (jft->hash != NULL && jft->md5 != NULL) {
    if (g_strcmp0(jft->hash, g_checksum_get_string(jft->md5))) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: File corrupt (%s)",
                   jft->name);
    } else {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: transfer finished (%s)"
                   " and verified", jft->name);
    }
  } else {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: transfer finished (%s)"
                 " but not verified", jft->name);
  }

  g_checksum_free(jft->md5);

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
