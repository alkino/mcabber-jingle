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

#include "filetransfer.h"


gconstpointer jingle_ft_check(JingleContent *cn, GError **err);
void jingle_ft_handle(JingleAction act, gconstpointer data, LmMessageNode *node);
gboolean jingle_ft_handle_data(gconstpointer data, const gchar *data2, guint len);
static gboolean is_md5_hash(const gchar *hash);
static void jingle_ft_init(void);
static void jingle_ft_uninit(void);

static guint file_cid = 0;

const gchar *deps[] = { "jingle", NULL };

JingleAppFuncs funcs = {
  jingle_ft_check,
  jingle_ft_handle,
  jingle_ft_handle_data
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
  ft->size = g_ascii_strtoll(sizestr, NULL, 10);

  // the size attribute is a xs:integer an therefore can be negative.
  if (ft->size < 0) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the offered file has a negative size");
    g_free(ft);
    return NULL;
  }

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

gboolean jingle_ft_handle_data(gconstpointer data, const gchar *data2, guint len)
{
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

gboolean handle_data(gconstpointer jingleft, const gchar *data, guint len)
{
  JingleFT *ft = (JingleFT *) jingleft;
  GError *err = NULL;
  GIOStatus status;
  gsize bytes_written = 0;

  // TODO: check if the file already exist or if it was created
  // during the call to jingle_ft_check and handle_data
  if (ft->outfile == NULL) {
    ft->outfile = g_io_channel_new_file(ft->name, "w", &err);
    if (ft->outfile == NULL || err != NULL) {
      // propagate the GError ?
      return FALSE;
	}
  }
  status = g_io_channel_write_chars(ft->outfile, data, (gssize) len,
                                    &bytes_written, &err);

  if (status != G_IO_STATUS_NORMAL || err != NULL) {
    return FALSE;
  }
  if (bytes_written != len) {
    // not supposed to happen if status is normal, unless outfile is non-blocking
    return FALSE;
  }
  return TRUE;
}

static void do_file(char *arg)
{
  char **args = split_arg(arg, 2, 0);
  
  if (!args[1]) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: give me a name!");
    return;
  }
  
  if (!g_file_test (args[1], G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {
    scr_LogPrint(LPRINT_LOGNORM, "File doesn't exist!");
    return;
  }
  
  if (!g_strcmp0(args[0], "send")) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: try to sent %s!",
                 args[1]);
    // Create a new session for send a file
    {
      JingleSession *sess;
      GChecksum *md5 = g_checksum_new(G_CHECKSUM_MD5);
      gchar *sid = new_sid();
      guchar data[1024];
      gsize bytes_read;
      gchar *jid;
      GSList *el;
      const gchar *jid = settings_opt_get("jid");
      JingleFT *jft = g_new0(JingleFT, 1);
      sess = session_new(sid, jid, jid);
      session_add_content(sess, "file", JINGLE_SESSION_STATE_PENDING);
      
      jft->desc = g_strdup(args[2]);
      jft->type = JINGLE_FT_OFFER;
      jft->name = g_path_get_basename(args[1]);
      jft->date = 0;
      jft->size = 0;
      jft->outfile = g_io_channel_new_file (args[1], "r", NULL);
      g_io_channel_set_encoding(jft->outfile, NULL, NULL);
      // For the md5 and the size
      while (g_io_channel_read_chars(jft->outfile,
                                     (gchar*)data, 1024, &bytes_read, NULL)
             != G_IO_STATUS_EOF) {
        jft->size+=bytes_read;
        g_checksum_update(md5, data, bytes_read);
      }
      jft->hash = g_strdup(g_checksum_get_string(md5));
      g_io_channel_seek_position (jft->outfile, 0, G_SEEK_SET, NULL);
      session_add_app(sess, "file", NS_JINGLE_APP_FT, jft);
      
      jid = CURRENT_JID;
        
      el = get_sorted_resources(jid);
      if (el == NULL)
        return;

      jid = g_strdup_printf("%s/%s", jid, (gchar*)el->data);
      jingle_handle_app(sess, "file", NS_JINGLE_APP_FT, jft, jid);
      
      free_gslist_resources(el);
      g_checksum_free(md5);
      g_free(sid);
    }  
  } else if (!g_strcmp0(args[0], "request")) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle File Transfer: try to request %s!",
                 args[1]);
    //later
  }
  
  
  
  free_arg_lst(args);
}

void jingle_ft_handle(JingleAction act, gconstpointer data, LmMessageNode *node)
{
  if (act == JINGLE_SESSION_INITIATE) {
    JingleFT *jft = (JingleFT*) data;
    if (lm_message_node_get_child(node, "description") != NULL)
      return;
  
    LmMessageNode *node2 = lm_message_node_add_child(node, "description", NULL);
    lm_message_node_set_attribute(node2, "xmlns", NS_JINGLE_APP_FT);
    if (jft->type == JINGLE_FT_OFFER)
      node2 = lm_message_node_add_child(node2, "offer", NULL);
    else
      node2 = lm_message_node_add_child(node2, "request", NULL);
    
    node2 = lm_message_node_add_child(node2, "file", NULL);
    
    lm_message_node_set_attributes(node2, "xmlns", NS_SI_FT, "name", jft->name,
                                   "size", jft->size, NULL);
    if (jft->hash != NULL)
      lm_message_node_set_attribute(node2, "hash", jft->hash);
    
    if (jft->desc != NULL)
      lm_message_node_add_child(node2, "desc", jft->desc);
    
    //if (jft->data != 0)
      
  }
}

static void jingle_ft_init(void)
{
  jingle_register_app(NS_JINGLE_APP_FT, &funcs, JINGLE_TRANSPORT_STREAMING);
  xmpp_add_feature(NS_JINGLE_APP_FT);
  /* Create completions */
  file_cid = compl_new_category();
  if (file_cid) {
    compl_add_category_word(file_cid, "send");
    compl_add_category_word(file_cid, "request");
  }
  /* Add command */
  cmd_add("file", "Send / Request a file", file_cid, 0, do_file, NULL);
}

static void jingle_ft_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_APP_FT);
  jingle_unregister_app(NS_JINGLE_APP_FT);
  cmd_del("file");
  if (file_cid)
    compl_del_category(file_cid);
}
