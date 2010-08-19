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
#include <gio/gio.h>

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

static gconstpointer check(JingleContent *cn, GError **err);
static void tomessage(gconstpointer data, LmMessageNode *node);
static void send(session_content *sc, gconstpointer data, gchar *buf, gsize size);
static void init(session_content *sc, gconstpointer data);
static void end(session_content *sc, gconstpointer data);

static void handle_sock_io(GSocket *sock, GIOCondition cond, gpointer data);
static void jingle_socks5_init(void);
static void jingle_socks5_uninit(void);


const gchar *deps[] = { "jingle", NULL };

static JingleTransportFuncs funcs = {
  .check     = check,
  .tomessage = tomessage,
  .new       = NULL,
  .send      = send,
  .init      = init,
  .end       = end
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


static gint index_in_array(const gchar *str, const gchar **array)
{
  gint i;
  for (i = 0; array[i]; i++) {
    if (!g_strcmp0(array[i], str)) {
      return i;
    }
  }
  return -1;
}

static gconstpointer check(JingleContent *cn, GError **err)
{
  JingleS5B *js5b;
  LmMessageNode *node = cn->transport, *node2;
  const gchar *modestr;

  js5b = g_new0(JingleS5B, 1);
  modestr    = lm_message_node_get_attribute(node, "mode");
  js5b->mode = index_in_array(modestr, jingle_s5b_modes);
  js5b->sid  = g_strdup(lm_message_node_get_attribute(node, "sid"));

  if (!js5b->sid) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the transport element is missing");
    g_free(js5b);
    return NULL;
  }

  for (node2 = node->children; node2; node2 = node2->next) {
    if (!g_strcmp0(node->name, "candidate")) {
      const gchar *portstr, *prioritystr, *typestr;
      S5BCandidate *jc = g_new0(S5BCandidate, 1);
      jc->cid      = g_strdup(lm_message_node_get_attribute(node2, "cid"));
      jc->host     = g_strdup(lm_message_node_get_attribute(node2, "host"));
      jc->jid      = g_strdup(lm_message_node_get_attribute(node2, "jid"));
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

static void tomessage(gconstpointer data, LmMessageNode *node)
{
  JingleS5B *js5 = (JingleS5B *)data;
  S5BCandidate *js5c;
  
  LmMessageNode *node2, *node3;
  gchar *port;
  gchar *priority;
  GSList *el;
  
  if (lm_message_node_get_child(node, "transport") != NULL)
    return;
  
  node2 = lm_message_node_add_child(node, "transport", NULL);

  lm_message_node_set_attributes(node2, "xmlns", NS_JINGLE_TRANSPORT_SOCKS5,
                                 "sid", js5->sid,
                                 "mode", jingle_s5b_modes[js5->mode],
                                 NULL);
  for (el = js5->candidates; el; el = el->next) {
    js5c = (S5BCandidate*) el->data;
    node3 = lm_message_node_add_child(node2, "candidate", NULL);
    
    port = g_strdup_printf("%" G_GUINT16_FORMAT, js5c->port);
    priority = g_strdup_printf("%" G_GUINT64_FORMAT, js5c->priority);
    
    lm_message_node_set_attributes(node3, "cid", js5c->cid,
                                   "host", js5c->host,
                                   "jid", js5c->jid,
                                   "port", port,
                                   "priority", priority,
                                   "type", jingle_s5b_types[js5c->type],
                                   NULL);
    g_free(port);
    g_free(priority);
  }
}

static void init(session_content *sc, gconstpointer data)
{
  JingleS5B *js5 = (JingleS5B *)data;
  GInetAddress *addr;
  GSocketAddress *saddr;
  GSource *socksource;
  GError *err = NULL;
  g_assert(js5->sock == NULL);

  addr = g_inet_address_new_from_string("127.0.0.1");
  js5->sock = g_socket_new(g_inet_address_get_family(addr), G_SOCKET_TYPE_STREAM,
                           G_SOCKET_PROTOCOL_TCP, &err);
  if (js5->sock == NULL) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle SOCKS5: Error while creating a new socket: %s",
                 err->message != NULL ? err->message : "(no message)");
    return; // TODO: we need a way to return errors...
  }
  g_socket_set_blocking(js5->sock, FALSE);
  socksource = g_socket_create_source(js5->sock, ~0, NULL);

  g_source_set_callback(socksource, (GSourceFunc)handle_sock_io, NULL, NULL);
  g_source_attach(socksource, NULL);
  g_source_unref(socksource);

  saddr = g_inet_socket_address_new(addr, 31337);
  if (!g_socket_connect(js5->sock, saddr, NULL, &err)) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle SOCKS5: Error while connecting to the host: %s",
                 err->message != NULL ? err->message : "(no message)");
    return;
  }

}

/**
 * Handle any event on a sock
 */
static void handle_sock_io(GSocket *sock, GIOCondition cond, gpointer data)
{
  switch (cond) {
    case G_IO_IN:
      break;
    case G_IO_OUT:
      break;
    case G_IO_ERR:
      break;
    case G_IO_HUP:
      break;
    default:
      ;
      // ?!
  }
}

static void jingle_socks5_init(void)
{
  jingle_register_transport(NS_JINGLE_TRANSPORT_SOCKS5, &funcs,
                            JINGLE_TRANSPORT_STREAMING,
                            JINGLE_TRANSPORT_PRIO_HIGH);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_SOCKS5);
}

static void jingle_socks5_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_TRANSPORT_SOCKS5);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_SOCKS5);
}
