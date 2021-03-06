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

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>
#include <jingle/sessions.h>
#include <jingle/send.h>

#include "filetransfer.h"


static gconstpointer newfrommessage(JingleContent *cn, GError **err);
static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err);
static void tomessage(gconstpointer data, LmMessageNode *node);
static gboolean handle_data(gconstpointer data, const gchar *data2, guint len);
static void start(session_content *sc);
static void send(session_content *sc);
static void stop(gconstpointer data);
static gchar* info(gconstpointer data);


static void jingle_ft_init(void);
static void jingle_ft_uninit(void);
// Return must be free
static gchar *_convert_size(guint64 size);
static int _next_index(void);
static void _free(JingleFT *jft);
static gboolean _check_hash(const gchar *hash1, GChecksum *md5);
static gboolean _is_md5_hash(const gchar *hash);
static void _jft_send(char **args, JingleFT *jft);
static void _jft_info(char **args);
static void _jft_flush(char **args);
static JingleFT* _new(const gchar *name);

const gchar *deps[] = { "jingle", NULL };

static GSList *info_list = NULL;
static guint jft_cid = 0;

const gchar* strstate[] = {
  "PENDING",
  "STARTING",
  "ENDING",
  "REJECT",
  "ERROR"
};

static JingleAppFuncs funcs = {
  .newfrommessage   = newfrommessage,
  .handle           = handle,
  .tomessage        = tomessage,
  .handle_data      = handle_data,
  .start            = start,
  .send             = send,
  .stop             = stop,
  .info             = info
};

module_info_t info_jingle_ft = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle File Transfer (XEP-0234)\n",
  .requires        = deps,
  .init            = jingle_ft_init,
  .uninit          = jingle_ft_uninit,
  .next            = NULL,
};


/**
 * @brief Check if a node description with xmlns of JFT is correct 
 *
 * @param cn the jinglecontent a node description
 * @param err contain an error of the domain JINGLE_CHECK_ERROR
 * @return a gconstpointer, which is a new allocated JingleFT
 */
static gconstpointer newfrommessage(JingleContent *cn, GError **err)
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
  ft->transmit = 0;
  ft->dir = JINGLE_FT_INCOMING;
  
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
  if (ft->hash != NULL && (strlen(ft->hash) != 32 || !_is_md5_hash(ft->hash))) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the offered file has an invalid hash");
    g_free(ft->name);
    g_free(ft);
    return NULL;
  }
  ft->hash = g_strndup(ft->hash, 32);

  {
    JingleFTInfo *jfti = g_new0(JingleFTInfo, 1);
    jfti->index = _next_index();
    jfti->jft = ft;
    info_list = g_slist_append(info_list, jfti);
  }

  return (gconstpointer) ft;
}

/**
 * @brief A function to handle incoming jingle action
 * @param action The action which have been received
 * @param data   Contain the JingleFT of the content concerned
 * @param node   The node himself
 * @return       TRUE if the action was handled, FALSE otherwise
 */
static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err)
{
  if (action == JINGLE_SESSION_INFO) {
    if (!g_strcmp0(lm_message_node_get_attribute(node, "xmlns"),
                   NS_JINGLE_APP_FT_INFO)
        && !g_strcmp0(node->name, "hash")) {
      ((JingleFT *)data)->hash = g_strdup(lm_message_node_get_value(node));
      return JINGLE_STATUS_HANDLED;
    }
    return JINGLE_STATUS_NOT_HANDLED;
  }
  return JINGLE_STATUS_NOT_HANDLED;
}

static gboolean _is_md5_hash(const gchar *hash)
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

  if (jft->dir != JINGLE_FT_INCOMING)
    return FALSE;

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
    jft->state = JINGLE_FT_STARTING;
    status = g_io_channel_set_encoding(jft->outfile, NULL, &err);
    if (status != G_IO_STATUS_NORMAL || err != NULL) {
     scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                  jft->name);
     g_error_free(err);
     return FALSE;
   }
  }
  
  jft->state = JINGLE_FT_STARTING;

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
  
  jft->transmit += len;
  return TRUE;
}


static int _next_index(void)
{
  static int a = 0;
  return ++a;
}

static void _jft_info(char **args)
{
  GSList *el = info_list;

  if (!info_list)
    scr_LogPrint(LPRINT_LOGNORM, "JFT: no file");

  for (el = info_list; el; el = el->next) {
    JingleFTInfo *jftio = el->data;
    JingleFT *jft = jftio->jft;
    gchar *strsize = _convert_size(jft->size);
    const gchar *dir = (jft->dir == JINGLE_FT_INCOMING) ? "<==" : "-->";
    gfloat percent = (gfloat)jft->size ? 
                       ((gfloat)jft->transmit / (gfloat)jft->size) * 100 :
                       0;
    const gchar *state = strstate[jft->state];
    const gchar *desc = jft->desc ? jft->desc : "";
    const gchar *hash = "";
    if (jft->dir == JINGLE_FT_INCOMING &&
        jft->state == JINGLE_FT_ENDING) {
      if (_check_hash(jft->hash, jft->md5) == FALSE)
        hash = "corrupt";
      else
        hash = "checked";
    }

    scr_LogPrint(LPRINT_LOGNORM, "[%i]%s %s %s %.2f%%: %s %s %s", jftio->index, 
                 dir, jftio->jft->name, strsize, percent, desc, state, hash);
    g_free(strsize);
  }
}

static void _jft_flush(char **args)
{
  GSList *el, *el2 = info_list;
  int count = 0;
  el = info_list;
  while (el) {
    JingleFTInfo *jftinf;
    jftinf = el->data;
    if (jftinf->jft->state == JINGLE_FT_ERROR ||
        jftinf->jft->state == JINGLE_FT_REJECT ||
        jftinf->jft->state == JINGLE_FT_ENDING) {
      count++;
      _free(jftinf->jft);
      info_list = g_slist_delete_link(info_list, el);
      if (info_list == NULL)
        break;
      el = el2;
      continue;
    }
    el2 = el;
    el = el->next;
  }
  scr_LogPrint(LPRINT_LOGNORM, "JFT: %i file%s removed", count, (count>1) ? "s" : "");
}

static JingleFT* _new(const gchar *name)
{
  struct stat fileinfo;
  GError *err = NULL;
  gchar *filename = expand_filename(name); // expand ~ to HOME
  JingleFT *jft = g_new0(JingleFT, 1);
  
  jft->desc = g_strdup(name);
  jft->type = JINGLE_FT_OFFER;
  jft->name = g_path_get_basename(filename);
  jft->transmit = 0;
  jft->hash = NULL;
  jft->md5 = NULL;
  jft->state = JINGLE_FT_PENDING;
  jft->dir = JINGLE_FT_OUTGOING;
  jft->date = 0;
  jft->size = 0;
  
  // Add the jft to the list
  JingleFTInfo *jftinf = g_new0(JingleFTInfo, 1);
  jftinf->index = _next_index();
  jftinf->jft = jft;
  info_list = g_slist_append(info_list, jftinf);

  if (g_stat(filename, &fileinfo) != 0) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: unable to stat %s",
                 filename);
    jft->state = JINGLE_FT_ERROR;
    return NULL;
  }

  if (!S_ISREG(fileinfo.st_mode) || S_ISLNK(fileinfo.st_mode)) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: File doesn't exist!");
    jft->state = JINGLE_FT_ERROR;
    return NULL;
  }

  jft->date = fileinfo.st_mtime;
  jft->size = fileinfo.st_size;
  
  jft->outfile = g_io_channel_new_file(filename, "r", &err);
  if (jft->outfile == NULL || err != NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                 name);
    g_error_free(err);
    jft->state = JINGLE_FT_ERROR;
    return NULL;
  }

  g_io_channel_set_encoding(jft->outfile, NULL, &err);
  if (jft->outfile == NULL || err != NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s %s", err->message,
                 name);
    g_error_free(err);
    jft->state = JINGLE_FT_ERROR;
    return NULL;
  }
  
  return jft;
}

static void _jft_send(char **args, JingleFT *jft2)
{
  JingleFT *jft = jft2;

  if (jft == NULL && !args[1]) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a name!");
    return;
  }

  scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Trying to send %s",
               args[1]);

  if (jft == NULL && (jft = _new(args[1])) == NULL)
    return;
  
  {
    gchar *ressource, *recipientjid;
    const gchar *namespaces[] = {NS_JINGLE, NS_JINGLE_APP_FT, NULL};

    if (CURRENT_JID == NULL) { // CURRENT_JID = the jid of the user which has focus
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Please, choose a valid JID in the roster");
      jft->state = JINGLE_FT_ERROR;
      return;
    }
    ressource = jingle_find_compatible_res(CURRENT_JID, namespaces);
    if (ressource == NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Cannot send file, because this buddy"
                                   " has no compatible ressource available");

      jft->state = JINGLE_FT_ERROR;
      return;
    }

    recipientjid = g_strdup_printf("%s/%s", CURRENT_JID, ressource);

    new_session_with_apps(recipientjid, (const gchar*[]){"file", NULL},
                          (gconstpointer[]){jft, NULL},
                          (const gchar*[]){NS_JINGLE_APP_FT, NULL});

    jingle_handle_app("file", NS_JINGLE_APP_FT, jft, recipientjid);
    
    g_free(recipientjid);
    g_free(ressource);
  }
}

static void _jft_retry(char **args)
{
  GSList *el;
  JingleFT *jft;
  JingleFTInfo *jftinf;
  gint index = g_ascii_strtoll(args[1], NULL, 10);
  for (el = info_list; el; el = el->next) {
    jftinf = el->data;
    if (jftinf->index == index);
      break;
  }
  
  if (!args[1]) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a number!");
    return;  
  }
  

  
  if (el == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a correct number!");
    return;  
  }
  
  jft = jftinf->jft;
  jft->hash = NULL;
  jft->size = 0;
  jft->transmit = 0;
  jft->outfile = NULL;
  jft->state = JINGLE_FT_PENDING;
  jft->desc = NULL;
  jft->md5 = NULL;
  
  _jft_send(args, jft);
}

static void do_sendfile(char *arg)
{
  char **args = split_arg(arg, 3, 0);

  if (!g_strcmp0(args[0], "send"))
    _jft_send(args, NULL);
  else if (!g_strcmp0(args[0], "info"))
    _jft_info(args);
  else if (!g_strcmp0(args[0], "flush"))
    _jft_flush(args);
  else if (!g_strcmp0(args[0], "retry"))
    _jft_retry(args);
  else
    scr_LogPrint(LPRINT_LOGNORM, "/jft: %s is not a correct option.", args[0]);

  free_arg_lst(args);
}

static void _free(JingleFT *jft)
{
  g_free(jft->hash);
  g_free(jft->name);
  g_free(jft->desc);
  if (jft->outfile != NULL)
    g_io_channel_unref(jft->outfile);
  if (jft->dir == JINGLE_FT_INCOMING)
    g_checksum_free(jft->md5);
  g_free(jft);
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

static void send_hash(const gchar *sid, const gchar *to, const gchar *hash)
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

  if (jft->dir != JINGLE_FT_OUTGOING)
    return;

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
    jft->state = JINGLE_FT_ERROR;
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s", err->message);
    g_error_free(err);
    return;
  }
  
  if (status == G_IO_STATUS_NORMAL) {
    jft->transmit += read;
    g_checksum_update(jft->md5, (guchar*)buf, read);
    // Call a handle in jingle who will call the trans
    handle_app_data(sc->sid, sc->from, sc->name, buf, read);
  }
  
  if (status == G_IO_STATUS_EOF) {
    handle_app_data(sc->sid, sc->from, sc->name, NULL, 0);
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: transfer finish (%s)",
                 jft->name);
    jft->hash = g_strdup(g_checksum_get_string(jft->md5));
    jft->state = JINGLE_FT_ENDING;
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
  jft->state = JINGLE_FT_STARTING;
  jft->md5 = g_checksum_new(G_CHECKSUM_MD5);
  
  scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Transfer start (%s)",
               jft->name);

  sc2->appfuncs->send(sc);
}

static gboolean _check_hash(const gchar *hash1, GChecksum *md5)
{
  const gchar *hash2;
  
  if (!hash1 || !md5)
    return FALSE;

  hash2 = g_checksum_get_string(md5);
  
  if (g_ascii_strncasecmp(hash1, hash2, 32))
    return FALSE;
  
  return TRUE;
}

// When we got a session-terminate
static void stop(gconstpointer data)
{
  JingleFT *jft = (JingleFT*)data;
  GError *err = NULL;
  GIOStatus status;

  if (jft->outfile != NULL) {
    status = g_io_channel_shutdown(jft->outfile, TRUE, &err);
    if (status != G_IO_STATUS_NORMAL || err != NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: %s",
                   err->message);
      g_error_free(err);
    }
  }
  
  if (jft->transmit < jft->size) {
    jft->state = JINGLE_FT_ERROR;
    if (jft->dir == JINGLE_FT_INCOMING)
      scr_LogPrint(LPRINT_LOGNORM, "JFT: session have been closed before we"
                   "receive all the file: %s", jft->name);
    else
      scr_LogPrint(LPRINT_LOGNORM, "JFT: session have been closed before we"
                   "send all the file: %s", jft->name);
    return;
  }
  
  jft->state = JINGLE_FT_ENDING;

  if (jft->hash != NULL && jft->md5 != NULL) {
    if (_check_hash(jft->hash, jft->md5) == FALSE) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: File corrupt (%s)",
                   jft->name);
    } else {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Transfer finished (%s)"
                   " and verified", jft->name);
    }
  } else {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: Transfer finished (%s)"
                 " but not verified", jft->name);
  }
}

static gchar *_convert_size(guint64 size)
{
  gchar *strsize;
 
  if (size < 1024)
    strsize = g_strdup_printf("%" G_GUINT64_FORMAT " B", size);
  else if (size < 1048576)
    strsize = g_strdup_printf("%.2lf KiB", size/1024.0);
  else if (size < 1073741824)
    strsize = g_strdup_printf("%.2lf MiB", size/1048576.0);
  else
    strsize = g_strdup_printf("%.2lf GiB", size/1073741824.0);

  return strsize;
}

static gchar* info(gconstpointer data)
{
  JingleFT *jft = (JingleFT *)data;
  gchar *info, *strsize = _convert_size(jft->size);
  info = g_strdup_printf("JFT: Receive %s (%s)", jft->name, strsize);

  g_free(strsize);

  return info;
}

static void jingle_ft_init(void)
{
  jingle_register_app(NS_JINGLE_APP_FT, &funcs, JINGLE_TRANSPORT_STREAMING);
  xmpp_add_feature(NS_JINGLE_APP_FT);
  jft_cid = compl_new_category(0);
  if (jft_cid) {
    compl_add_category_word(jft_cid, "send");
    //compl_add_category_word(jft_cid, "request");
    compl_add_category_word(jft_cid, "info");
    compl_add_category_word(jft_cid, "flush");
  }
  /* Add command */
  cmd_add("jft", "Manage file transfer", jft_cid, 0, do_sendfile, NULL);
}

static void jingle_ft_uninit(void)
{
  g_slist_free(info_list);
  xmpp_del_feature(NS_JINGLE_APP_FT);
  jingle_unregister_app(NS_JINGLE_APP_FT);
  cmd_del("file");
  if (jft_cid)
    compl_del_category(jft_cid);
}
/* vim: set expandtab cindent cinoptions=>2\:2(0 sw=2 ts=2:  For Vim users... */
