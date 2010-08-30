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

#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <mcabber/xmpp.h>
#include <mcabber/modules.h>
#include <mcabber/utils.h>
#include <mcabber/xmpp_helper.h>
#include <mcabber/settings.h>
#include <mcabber/logprint.h>
#include <mcabber/hooks.h>

#include <jingle/jingle.h>
#include <jingle/check.h>
#include <jingle/register.h>

#include "socks5.h"

static gconstpointer newfrommessage(JingleContent *cn, GError **err);
static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err);
static void tomessage(gconstpointer data, LmMessageNode *node);
static gconstpointer new(void);
static void _send(session_content *sc, gconstpointer data, gchar *buf, gsize size);
static void init(session_content *sc, gconstpointer data);
static void end(session_content *sc, gconstpointer data);
static gchar *info(gconstpointer data);

static void connect_candidate(JingleS5B *js5b, S5BCandidate *cand);
static void connect_next_candidate(JingleS5B *js5b, S5BCandidate *cand);
static void
handle_listener_accept(GObject *_listener, GAsyncResult *res, gpointer data);
static void
handle_client_connect(GObject *_client, GAsyncResult *res, gpointer data);
static void handle_sock_io(GSocket *sock, GIOCondition cond, gpointer data);
static GSList *get_all_local_ips();
static gchar *gen_random_sid(void);
static gchar *gen_random_cid(void);
static void jingle_socks5_init(void);
static void jingle_socks5_uninit(void);


const gchar *deps[] = { "jingle", NULL };

static JingleTransportFuncs funcs = {
  .newfrommessage = newfrommessage,
  .handle         = handle,
  .tomessage      = tomessage,
  .new            = new,
  .send           = _send,
  .init           = init,
  .end            = end,
  .info           = info
};

module_info_t  info_jingle_s5b = {
  .branch          = MCABBER_BRANCH,
  .api             = MCABBER_API_VERSION,
  .version         = PROJECT_VERSION,
  .description     = "Jingle SOCKS5 Bytestream (XEP-0260)\n",
  .requires        = deps,
  .init            = jingle_socks5_init,
  .uninit          = jingle_socks5_uninit,
  .next            = NULL,
};

static const gchar *jingle_s5b_types[] = {
  "direct",
  "assisted",
  "tunnel",
  "proxy",
  NULL
};

static const gchar *jingle_s5b_modes[] = {
  "tcp",
  "udp",
  NULL
};

typedef struct {
  GInetAddress *address;
  guint32       priority;
  JingleS5BType type;
} LocalIP;

/**
 * @brief Linked list of candidates to send on session-initiate
 */
static GSList *local_ips = NULL;


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

static gint prioritycmp(gconstpointer a, gconstpointer b)
{
  S5BCandidate *s1 = (S5BCandidate *)a, *s2 = (S5BCandidate *)b;
  if (s1->priority < s2->priority) {
    return 1;
  } else if (s1->priority > s2->priority) {
    return -1;
  } else {
    return 0;
  }
}

/**
 * @brief Parse a list of <candidate> elements
 * @return a list of S5BCandidate
 */
static GSList *parse_candidates(LmMessageNode *node)
{
  LmMessageNode *node2;
  GSList *list = NULL;

  for (node2 = node->children; node2; node2 = node2->next) {
    if (g_strcmp0(node->name, "candidate"))
        continue;
    const gchar *hoststr, *portstr, *prioritystr, *typestr;
    S5BCandidate *cand = g_new0(S5BCandidate, 1);
    cand->cid    = g_strdup(lm_message_node_get_attribute(node2, "cid"));
    cand->jid    = g_strdup(lm_message_node_get_attribute(node2, "jid"));
    hoststr      = lm_message_node_get_attribute(node2, "host");
    portstr      = lm_message_node_get_attribute(node2, "port");
    prioritystr  = lm_message_node_get_attribute(node2, "priority");
    typestr      = lm_message_node_get_attribute(node2, "type");

    if (!cand->cid || !hoststr || !cand->jid || !prioritystr) {
      g_free(cand);
      continue;
    }
    cand->host     = g_inet_address_new_from_string(hoststr);
    cand->port     = g_ascii_strtoull(portstr, NULL, 10);
    cand->priority = g_ascii_strtoull(prioritystr, NULL, 10);
    cand->type     = index_in_array(typestr, jingle_s5b_types);

    if (cand->type == -1 || cand->host == NULL) {
      g_free(cand);
      continue;
    }

    list = g_slist_prepend(list, cand);
  }
  list = g_slist_sort(list, prioritycmp);
  return list;
}

static GSList *get_our_candidates(guint16 port)
{
  GSList *our_candidates = NULL, *entry;

  for (entry = local_ips; entry; entry = entry->next) {
    LocalIP *lcand = (LocalIP *)entry->data;
    S5BCandidate *cand = g_new0(S5BCandidate, 1);
    cand->cid      = gen_random_cid();
    cand->host     = g_object_ref(lcand->address);
    cand->jid      = g_strdup(lm_connection_get_jid(lconnection));
    cand->port     = port;
    cand->priority = lcand->priority;

    our_candidates = g_slist_prepend(our_candidates, cand);
  }
  our_candidates = g_slist_sort(our_candidates, prioritycmp);
  return our_candidates;
}

/**
 * @brief Get a port number by settings or randomly
 * @return A guint16 containing the port number
 * */
static guint16 get_port(void)
{
  // TODO: find a way to make sure the port is not already used
  guint64 portstart, portend;
  guint16 port;
  const gchar *port_range = settings_opt_get("js5b_portrange");

  if (port_range != NULL) {
    sscanf(port_range, "%" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT, &portstart, &portend);

    if ((portstart >= 1024 && portstart <= (guint16)~0) &&
        (portend >= 1024 && portend <= (guint16)~0) && portstart <= portend) {
      port = g_random_int_range(portstart, portend);
    } else {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Invalid port range specified");
      port = g_random_int_range(1024, (guint16)~0);
    }
  } else {
    port = g_random_int_range(1024, (guint16)~0);
  }

  return port;
}

static gconstpointer newfrommessage(JingleContent *cn, GError **err)
{
  JingleS5B *js5b;
  LmMessageNode *node = cn->transport;
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

  js5b->candidates = parse_candidates(node);
  js5b->ourcandidates = get_our_candidates(get_port());

  return (gconstpointer) js5b;
}

static gconstpointer new(void)
{
  JingleS5B *js5b = g_new0(JingleS5B, 1);

  js5b->mode = JINGLE_S5B_TCP;
  js5b->sid  = gen_random_sid();

  js5b->ourcandidates = get_our_candidates(get_port());

  return js5b;
}

static JingleHandleStatus
handle_session_accept(JingleS5B *js5b, LmMessageNode *node, GError **err)
{
  js5b->candidates = parse_candidates(node);
  return JINGLE_STATUS_HANDLED;
}

static JingleHandleStatus handle(JingleAction action, gconstpointer data,
                                 LmMessageNode *node, GError **err)
{
  JingleS5B *js5b = (JingleS5B *)data;

  if (action == JINGLE_SESSION_ACCEPT) {
    return handle_session_accept(js5b, node, err);
  }
  return JINGLE_STATUS_NOT_HANDLED;
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
  for (el = js5->ourcandidates; el; el = el->next) {
    js5c = (S5BCandidate*) el->data;
    node3 = lm_message_node_add_child(node2, "candidate", NULL);
    
    port = g_strdup_printf("%" G_GUINT16_FORMAT, js5c->port);
    priority = g_strdup_printf("%" G_GUINT64_FORMAT, js5c->priority);
    
    lm_message_node_set_attributes(node3, "cid", js5c->cid,
                                   "host", g_inet_address_to_string(js5c->host),
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
  JingleS5B *js5b = (JingleS5B *)data;
  GSocketAddress *saddr;
  guint numlistening = 0; // number of addresses we are listening to
  GSList *entry;
  GError *err = NULL;
  GSocket *sock;

  // First, we listen on all ips
  js5b->listener = g_socket_listener_new();
  for (entry = js5b->ourcandidates; entry; entry = entry->next) {
    S5BCandidate *cand = (S5BCandidate *)entry->data;

    sock = g_socket_new(g_inet_address_get_family(cand->host),
                        G_SOCKET_TYPE_STREAM,
                        G_SOCKET_PROTOCOL_TCP, &err);
    if (sock == NULL) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Error while creating a new socket: %s",
                   err->message ? err->message : "(no message)");
      continue;
    }
    g_socket_set_blocking(sock, FALSE);

    saddr = g_inet_socket_address_new(cand->host, cand->port);
    if (!g_socket_bind(sock, saddr, TRUE, &err)) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Unable to bind a socket on %s port %u: %s",
                   g_inet_address_to_string(cand->host),
                   cand->port,
                   err->message ? err->message : "(no message)");
      goto cleancontinue;
    }

    if (!g_socket_listen(sock, &err)) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Unable to listen on %s port %u: %s",
                   g_inet_address_to_string(cand->host),
                   cand->port,
                   err->message ? err->message : "(no message)");
      goto cleancontinue;
    }

    if (!g_socket_listener_add_socket(js5b->listener, sock, NULL, &err)) {
      scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Unable to add our socket to the"
                   " GSocketListener: %s",
                   err->message ? err->message : "(no message)");
      goto cleancontinue;
	}

    scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Listening on %s:%u",
                 g_inet_address_to_string(cand->host),
                 cand->port);
	++numlistening;

cleancontinue:
      if (err != NULL) g_clear_error(&err);
      g_object_unref(saddr);
      g_object_unref(sock);
  }

  if (numlistening > 0) {
    g_socket_listener_accept_async(js5b->listener, NULL, handle_listener_accept, NULL);
  } else {
      g_object_unref(js5b->listener);
  }

  // Then, we start connecting to the other entity's candidates, if any.
  if (js5b->candidates) {
    js5b->client = g_socket_client_new();
    S5BCandidate *cand = (S5BCandidate *)js5b->candidates->data;
    connect_candidate(js5b, cand);
  }
}

/**
 * @brief Cancel an ongoing connection after 5 seconds
 * @param data  A GPtrArray
 * 
 * "A client SHOULD NOT wait for a TCP timeout on connect.
 * If it is unable to connect to any candidate within 5 seconds
 * it SHOULD send a candidate-error to the other party."
 */
static gboolean connect_cancel_timeout(gpointer data)
{
  GPtrArray *args = (GPtrArray *)data;
  JingleS5B *js5b = g_ptr_array_index(args, 0);
  //S5BCandidate *cand = g_ptr_array_index(args, 1);
  g_ptr_array_unref(args);

  g_cancellable_cancel(js5b->cancelconnect);
  return FALSE;
}

static void connect_candidate(JingleS5B *js5b, S5BCandidate *cand)
{
  GSocketAddress *saddr;
  GPtrArray *args;
  guint eventid;

  args = g_ptr_array_sized_new(2);
  g_ptr_array_add(args, js5b);
  g_ptr_array_add(args, cand);

  saddr = g_inet_socket_address_new(cand->host, cand->port);
  js5b->cancelconnect = g_cancellable_new();

  eventid = g_timeout_add_seconds(5, connect_cancel_timeout, g_ptr_array_ref(args));
  g_ptr_array_add(args, GUINT_TO_POINTER(eventid));
  g_socket_client_connect_async(js5b->client, G_SOCKET_CONNECTABLE(saddr),
                                js5b->cancelconnect, handle_client_connect, args);
  g_object_unref(saddr);
}

/**
 * Convenience function that find the next candidate to try and
 * call connect_candidate
 */
static void connect_next_candidate(JingleS5B *js5b, S5BCandidate *cand)
{
  GSList *link = g_slist_find(js5b->candidates, cand);
  if (js5b->cancelconnect != NULL)
    g_object_unref(js5b->cancelconnect);
  g_assert(link != NULL);
  if (link->next == NULL) {
    // there is no next candidate to try.
  }
  connect_candidate(js5b, (S5BCandidate *)link->next->data);
  return;
}

static gchar *info(gconstpointer data)
{
  //JingleS5B *js5b = (JingleS5B *)data;
  gchar *info = g_strdup_printf("S5B");
  return info;
}

static void end(session_content *sc, gconstpointer data) {
  return;
}

/**
 * @brief Handle incoming connections
 */
static void
handle_listener_accept(GObject *_listener, GAsyncResult *res, gpointer data)
{
  GError *err = NULL;
  GSocketConnection *conn;
  //scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Got Incoming Connection");
  conn = g_socket_listener_accept_finish(G_SOCKET_LISTENER(_listener), res, NULL, &err);
}

/**
 * @brief Handle outgoing connections
 */
static void
handle_client_connect(GObject *_client, GAsyncResult *res, gpointer data)
{
  GError *err = NULL;
  GSocketConnection *conn;
  GPtrArray *args;
  JingleS5B *js5b;
  S5BCandidate *cand;
  //scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Got Outgoing Connection");
  
  args = (GPtrArray *)data;
  js5b = g_ptr_array_index(args, 0);
  cand = g_ptr_array_index(args, 1);
  g_ptr_array_unref(args);

  conn = g_socket_client_connect_finish(G_SOCKET_CLIENT(_client), res, &err);

  if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    // if we did not received a CANCELLED error, then the time limit was not
    // reached and we need to clean up the GSource and unref args a second time.
    guint eventid = GPOINTER_TO_UINT(g_ptr_array_index(args, 2));
    GSource *s = g_main_context_find_source_by_id(NULL, eventid);
    g_source_destroy(s);
    g_ptr_array_unref(args);
    connect_next_candidate(js5b, cand);
    // we need to send a candidate-error in case we cannot connect.
    return;
  }

  if (err != NULL) {
    if (err->domain == G_IO_ERROR)
      scr_LogPrint(LPRINT_DEBUG, "Jingle S5B: IO Error (%s)",
                   err->message ? err->message : "no message");
    else
      scr_LogPrint(LPRINT_DEBUG, "Jingle S5B: %s Error (%s)",
                   g_quark_to_string (err->domain),
                   err->message ? err->message : "no message");

    g_error_free(err);
    connect_next_candidate(js5b, cand);
    return;
  }
  js5b->connection = conn;
  // we have a valid connection
}

/**
 * @brief Handle any event on a sock
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

static void _send(session_content *sc, gconstpointer data, gchar *buf, gsize size)
{
  return;
}

/**
 * @brief Discover all IPs of this computer
 * @return A linked list of GInetAddress
 */
static GSList *get_all_local_ips(void) {
  GSList *addresses = NULL;
  GInetAddress *thisaddr;
  GSocketFamily family;
  struct ifaddrs *first, *ifaddr;
  struct sockaddr_in *native;
  struct sockaddr_in6 *native6;
  const guint8 *addrdata;
  guint16 ifacecounter = 0; // for lack of a better method
  LocalIP *candidate;
  gchar **ifblacklist;
  guint ifblkcnt;

  gint rval = getifaddrs(&first);
  if (rval != 0) {
    scr_LogPrint(LPRINT_LOGNORM, "Jingle S5B: Unable to retreive local ip addresses");
    return NULL;
  }

  if (settings_opt_get("js5b_iface_blacklist") != NULL) {
    ifblacklist = g_strsplit(settings_opt_get("js5b_iface_blacklist"), ",", 0);
  } else {
    ifblacklist = (gchar*[]){NULL};
  }

  for (ifaddr = first; ifaddr; ifaddr = ifaddr->ifa_next) {
    gboolean continueloop = FALSE;
    if (!(ifaddr->ifa_flags & IFF_UP) || ifaddr->ifa_flags & IFF_LOOPBACK)
      continue;

    for (ifblkcnt = 0; ifblacklist[ifblkcnt]; ifblkcnt++)
      if (!g_strcmp0(ifaddr->ifa_name, ifblacklist[ifblkcnt])) {
        continueloop = TRUE;
        break;
      }
    
    if (continueloop) continue;

    if (ifaddr->ifa_addr->sa_family == AF_INET) {
      native = (struct sockaddr_in *)ifaddr->ifa_addr;
      addrdata = (const guint8 *)&native->sin_addr.s_addr;
      family = G_SOCKET_FAMILY_IPV4;
    } else if (ifaddr->ifa_addr->sa_family == AF_INET6) {
      native6 = (struct sockaddr_in6 *)ifaddr->ifa_addr;
      addrdata = (const guint8 *)&native6->sin6_addr.s6_addr;
      family = G_SOCKET_FAMILY_IPV6;
    } else
      continue;

    thisaddr = g_inet_address_new_from_bytes(addrdata, family);
    if (g_inet_address_get_is_link_local(thisaddr)) {
      g_object_unref(thisaddr);
      continue;
    }/* else if (g_inset_address_get_is_site_local(thisaddr)) {
      // TODO: should we offer a way to filter the offer of LAN ips ?
    } */
    candidate = g_new0(LocalIP, 1);
    candidate->address  = thisaddr;
    candidate->priority = (1<<16)*126+ifacecounter;
    candidate->type     = JINGLE_S5B_DIRECT;
    addresses = g_slist_prepend(addresses, candidate);
    ++ifacecounter;
  }
  freeifaddrs(first);

  return addresses;
}

static gchar *random_str(guint len)
{
  gchar *str;
  gchar car[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  gint i;
  str = g_new0(gchar, 8);
  for (i = 0; i < len; i++)
    str[i] = car[g_random_int_range(0, sizeof(car)/sizeof(car[0])-1)];

  str[len] = '\0';
  return str;	
}

static gchar *gen_random_sid(void)
{
  return random_str(7);
}

static gchar *gen_random_cid(void)
{
  return random_str(7);
}

static void free_localip(LocalIP *l) {
  g_object_unref(l->address);
  g_free(l);
}

static void jingle_socks5_init(void)
{
  // ugly hack to fix the segfault when quitting:
  // mcabber doesn't load gthread or gobject but they are required by gio,
  // and cannot be unloaded once they are loaded or a segfault occur.
  // We dlopen gio with global | nodelete flags. This will also load gobject
  // and gthread as dependencies. g_type_init will init gobject/gthread (and
  // set threads_got_initialized to true).
  if (g_threads_got_initialized == FALSE) {
    void *dlopen(const char *filename, int flag);
    // RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE
    dlopen("libgio-2.0.so", 0x00001 | 0x00100 | 0x01000);
    g_type_init();
  }
  jingle_register_transport(NS_JINGLE_TRANSPORT_SOCKS5, &funcs,
                            JINGLE_TRANSPORT_STREAMING,
                            JINGLE_TRANSPORT_PRIO_HIGH);
  xmpp_add_feature(NS_JINGLE_TRANSPORT_SOCKS5);
  local_ips = get_all_local_ips();
}

static void jingle_socks5_uninit(void)
{
  xmpp_del_feature(NS_JINGLE_TRANSPORT_SOCKS5);
  jingle_unregister_transport(NS_JINGLE_TRANSPORT_SOCKS5);
  g_slist_foreach(local_ips, (GFunc)free_localip, NULL);
  g_slist_free(local_ips);
}
