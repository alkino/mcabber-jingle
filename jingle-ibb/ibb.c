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


gconstpointer jingle_ibb_check(JingleContent *cn, GError **err);
static void jingle_ibb_init(void);
static void jingle_ibb_uninit(void);


const gchar *deps[] = { "jingle", NULL };

JingleTransportFuncs funcs = {jingle_ibb_check, NULL};

module_info_t info_jingle_filetransfer = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle In Band Bytestream (XEP-0261)\n",
  .requires        = deps,
  .init            = jingle_ibb_init,
  .uninit          = jingle_ibb_uninit,
  .next            = NULL,
};


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
  if (ibb->blocksize < 0) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "block-size is negative");
    g_free(ibb);
    return NULL;
  }

  return (gconstpointer) ibb;
}

static void jingle_ibb_init(void)
{
  jingle_register_transport(NS_JINGLE_TRANSPORT_IBB, &funcs, JINGLE_TRANS_IN_BAND, JINGLE_TRANS_TCP);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_IBB);
}

static void jingle_ibb_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_TRANSPORT_IBB);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_IBB);
}
