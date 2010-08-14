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

module_info_t  info_jingle_socks5bytestream = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle Socks5 Bytestream (XEP-0260)\n",
  .requires        = deps,
  .init            = jingle_socks5_init,
  .uninit          = jingle_socks5_uninit,
  .next            = NULL,
};

static const gchar *jingle_s5b_types[] = {
  "assisted",
  "direct",
  "proxy",
  "tunnel",
  NULL
};

static const gchar *jingle_s5b_modes[] = {
  "tcp",
  "udp",
  NULL
};


gint index_in_array(const gchar *str, const gchar **array)
{
  gint i;
  for (i = 0; array[i]; i++) {
    if (!g_strcmp0(array[i], str)) {
      return i;
    }
  }
  return -1;
}

const gchar* jingle_socks5_xmlns(void)
{
  return NS_JINGLE_TRANSPORT_SOCKS5;
}

gconstpointer jingle_socks5_check(JingleContent *cn, GError **err)
{
  JingleSocks5 *js5b;
  LmMessageNode *node = cn->transport, *node2;
  const gchar *modestr;

  js5b = g_new0(JingleSocks5, 1);
  modestr    = lm_message_node_get_attribute(node, "mode");
  js5b->mode = index_in_array(modestr, jingle_s5b_modes);
  js5b->sid  = lm_message_node_get_attribute(node, "sid");

  if (!js5b->sid) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the transport element is missing");
    g_free(js5b);
    return NULL;
  }

  for (node2 = node->children; node2; node2 = node2->next) {
    if (!g_strcmp0(node->name, "candidate")) {
      const gchar *portstr, *prioritystr, *typestr;
      JingleS5BCandidate *jc = g_new0(JingleS5BCandidate, 1);
      jc->cid      = lm_message_node_get_attribute(node2, "cid");
      jc->host     = lm_message_node_get_attribute(node2, "host");
      jc->jid      = lm_message_node_get_attribute(node2, "jid");
      portstr      = lm_message_node_get_attribute(node2, "port");
      prioritystr  = lm_message_node_get_attribute(node2, "priority");
      typestr      = lm_message_node_get_attribute(node2, "type");

      if (!jc->cid || !jc->host || !jc->jid || !prioritystr) {
        g_free(jc);
        continue;
	  }
      jc->port     = g_ascii_strtoull(portstr, NULL, 10);
      jc->priority = g_ascii_strtoull(prioritystr, NULL, 10);
      jc->type     = index_in_array(typestr, jingle_s5b_types);

      if (jc->type == -1) {
        g_free(jc);
        continue;
	  }

      js5b->candidates = g_slist_append(js5b->candidates, jc);
    }
  }

  return (gconstpointer) js5b;
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
