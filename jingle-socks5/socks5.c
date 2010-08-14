/*
 * socks5.c
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

#include <mcabber/xmpp.h>
#include <mcabber/modules.h>
#include <mcabber/utils.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/logprint.h>
#include <mcabber/hooks.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>

#include "socks5.h"

gconstpointer jingle_socks5_check(JingleContent *cn, GError **err);
gboolean jingle_socks5_cmp(gconstpointer data1, gconstpointer data2);
void jingle_socks5_tomessage(gconstpointer data, LmMessageNode *node);
const gchar* jingle_socks5_xmlns(void);
gconstpointer jingle_socks5_new(void);
void jingle_socks5_send(session_content *sc, const gchar *to, gconstpointer data, gchar *buf, gsize size);

static void jingle_socks5_init(void);
static void jingle_socks5_uninit(void);

const gchar *deps[] = { "jingle", NULL };

static JingleTransportFuncs funcs = {
  jingle_socks5_xmlns,
  jingle_socks5_check,
  jingle_socks5_tomessage,
  jingle_socks5_cmp,
  jingle_socks5_new,
  jingle_socks5_send
};

module_info_t  info_jingle_inbandbytestream = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle In Band Bytestream (XEP-0261)\n",
  .requires        = deps,
  .init            = jingle_socks5_init,
  .uninit          = jingle_socks5_uninit,
  .next            = NULL,
};


const gchar* jingle_socks5_xmlns(void)
{
  return NS_JINGLE_TRANSPORT_SOCKS5;
}

static void jingle_socks5_init(void)
{
  jingle_register_transport(NS_JINGLE_TRANSPORT_SOCKS5, &funcs,
                            JINGLE_TRANSPORT_STREAMING,
                            JINGLE_TRANSPORT_HIGH);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_SOCKS5);
}

static void jingle_socks5_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_TRANSPORT_SOCKS5);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_SOCKS5);
}
