/*
 * socks5-proto.c
 *
 * Based on the gio socks5 implementation that is not yet included in
 * the latest stable glib release.
 * Original Authors:
 * Youness Alaoui <youness.alaoui@collabora.co.uk>
 * Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
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

#include <glib.h>
#include <gio/gio.h>

#include <string.h>

#include "socks5-proto.h"


#define SOCKS5_VERSION            0x05

#define SOCKS5_CMD_CONNECT        0x01
#define SOCKS5_CMD_BIND           0x02
#define SOCKS5_CMD_UDP_ASSOCIATE  0x03

#define SOCKS5_ATYP_IPV4          0x01
#define SOCKS5_ATYP_DOMAINNAME    0x03
#define SOCKS5_ATYP_IPV6          0x04

#define SOCKS5_AUTH_VERSION       0x01

#define SOCKS5_AUTH_NONE          0x00
#define SOCKS5_AUTH_GSSAPI        0x01
#define SOCKS5_AUTH_USR_PASS      0x02
#define SOCKS5_AUTH_NO_ACCEPT     0xff

#define SOCKS5_MAX_LEN            255
#define SOCKS5_RESERVED           0x00

#define SOCKS5_REP_SUCCEEDED      0x00
#define SOCKS5_REP_SRV_FAILURE    0x01
#define SOCKS5_REP_NOT_ALLOWED    0x02
#define SOCKS5_REP_NET_UNREACH    0x03
#define SOCKS5_REP_HOST_UNREACH   0x04
#define SOCKS5_REP_REFUSED        0x05
#define SOCKS5_REP_TTL_EXPIRED    0x06
#define SOCKS5_REP_CMD_NOT_SUP    0x07
#define SOCKS5_REP_ATYPE_NOT_SUP  0x08

#define S5B_DST_PORT              0


/*
 * +----+----------+----------+
 * |VER | NMETHODS | METHODS  |
 * +----+----------+----------+
 * | 1  |    1     | 1 to 255 |
 * +----+----------+----------+
 */
#define SOCKS5_NEGO_MSG_LEN       4
static gint
client_set_nego_msg (guint8 *msg, gboolean has_auth)
{
  gint len = 3;

  msg[0] = SOCKS5_VERSION;
  msg[1] = 0x01; /* number of methods supported */
  msg[2] = SOCKS5_AUTH_NONE;

  /* add support for authentication method */
  if (has_auth) {
    msg[1] = 0x02; /* number of methods supported */
    msg[3] = SOCKS5_AUTH_USR_PASS;
    len++;
  }

  return len;
}

/*
 * +----+--------+
 * |VER | METHOD |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
static gint
server_set_nego_reply_msg (guint8 *msg, guint8 method)
{
  gint len = 2;

  msg[0] = SOCKS5_VERSION;
  msg[1] = method; /* selected method */

  return len;
}


/*
 * +----+--------+
 * |VER | METHOD |
 * +----+--------+
 * | 1  |   1    |
 * +----+--------+
 */
#define SOCKS5_NEGO_REP_LEN       2
static gboolean
client_parse_nego_reply (const guint8 *data,
                         gboolean     has_auth,
                         gboolean    *must_auth,
                         GError     **error)
{
  if (data[0] != SOCKS5_VERSION) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                         "The server is not a SOCKSv5 proxy server.");
    return FALSE;
  }

  switch (data[1]) {
    case SOCKS5_AUTH_NONE:
      *must_auth = FALSE;
      break;

    case SOCKS5_AUTH_USR_PASS:
      if (!has_auth) {
        g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_NEED_AUTH,
                             "The SOCKSv5 proxy requires authentication.");
        return FALSE;
      }
      *must_auth = TRUE;
      break;

    case SOCKS5_AUTH_GSSAPI:
    case SOCKS5_AUTH_NO_ACCEPT:
    default:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_AUTH_FAILED,
                           "The SOCKSv5 require an authentication method that is not "
                           "supported.");
      return FALSE;
      break;
  }

  return TRUE;
}


/*
 * +----+----------+----------+
 * |VER | NMETHODS | METHODS  |
 * +----+----------+----------+
 * | 1  |    1     | 1 to 255 |
 * +----+----------+----------+
 */
static gboolean
server_parse_nego_init (const guint8 *data,
                        gssize        datalen,
                        GError      **error)
{
  guint i;

  if (data[0] != SOCKS5_VERSION) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                         "The client is not a SOCKSv5 client.");
    return FALSE;
  }

  for (i = 2; i < datalen && i < data[1]; ++i) {
    guint8 method = data[i];
    if (method == 0x00)
      return TRUE;
  }
  
  g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_AUTH_FAILED,
                       "The client did not offer any authentication method "
                       "we support.");
  return FALSE;
}


#define SOCKS5_AUTH_MSG_LEN       515
static gint
set_auth_msg (guint8      *msg,
              const gchar *username,
              const gchar *password,
              GError **error)
{
  gint len = 0;
  gint ulen = 0; /* username length */
  gint plen = 0; /* Password length */

  if (username)
    ulen = strlen (username);

  if (password)
    plen = strlen (password);

  if (ulen > SOCKS5_MAX_LEN || plen > SOCKS5_MAX_LEN) {
    g_set_error (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                 "Username or password is too long for SOCKSv5 "
                 "protocol (max. is %i).",
                 SOCKS5_MAX_LEN);
    return FALSE;
  }

  msg[len++] = SOCKS5_AUTH_VERSION;
  msg[len++] = ulen;

  if (ulen > 0)
    memcpy (msg + len, username, ulen);

  len += ulen;
  msg[len++] = plen;

  if (plen > 0)
    memcpy (msg + len, password, plen);

  len += plen;

  return len;
}


static gboolean
check_auth_status (const guint8 *data, GError **error)
{
  if (data[0] != SOCKS5_VERSION
      || data[1] != SOCKS5_REP_SUCCEEDED) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_AUTH_FAILED,
                         "SOCKSv5 authentication failed due to wrong "
                         "username or password.");
    return FALSE;
  }
  return TRUE;
}

/*
 * +----+-----+-------+------+----------+----------+
 * |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 * DST.ADDR is a string with first byte being the size. So DST.ADDR may not be
 * longer then 256 bytes.
 */
#define SOCKS5_CONN_MSG_LEN       262
static gint
client_set_connect_msg (guint8       *msg,
                        const gchar *hostname,
                        guint16      port,
                        GError     **error)
{
  guint len = 0;

  msg[len++] = SOCKS5_VERSION;
  msg[len++] = SOCKS5_CMD_CONNECT;
  msg[len++] = SOCKS5_RESERVED;

  gsize host_len = strlen (hostname);

  if (host_len > SOCKS5_MAX_LEN) {
    g_set_error (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                 "Hostname '%s' too long for SOCKSv5 protocol "
                 "(maximum is %i bytes)",
                 hostname, SOCKS5_MAX_LEN);
    return -1;
  }

  msg[len++] = SOCKS5_ATYP_DOMAINNAME;
  msg[len++] = (guint8) host_len;
  memcpy (msg + len, hostname, host_len);
  len += host_len;

  {
    guint16 hp = g_htons (port);
    memcpy (msg + len, &hp, 2);
    len += 2;
  }

  return len;
}

static guint
server_parse_connect_msg (const guint8 *data, gint *atype, GError **error)
{
  if (data[0] != SOCKS5_VERSION) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                         "The client is not a SOCKSv5 client.");
    return FALSE;
  }

  if (data[1] != SOCKS5_CMD_CONNECT) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                         "The server only supports the CONNECT command.");
  }

  switch (data[3]) {
    case SOCKS5_ATYP_IPV4:
      *atype = SOCKS5_ATYP_IPV4;
      break;
    case SOCKS5_ATYP_DOMAINNAME:
      *atype = SOCKS5_ATYP_DOMAINNAME;
      break;
    case SOCKS5_ATYP_IPV6:
      *atype = SOCKS5_ATYP_IPV6;
      break;
    default:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                           "The client sent an invalid address type.");
      return FALSE;
  }

  return TRUE;
}

/*
 * +----+-----+-------+------+----------+----------+
 * |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 * +----+-----+-------+------+----------+----------+
 * | 1  |  1  | X'00' |  1   | Variable |    2     |
 * +----+-----+-------+------+----------+----------+
 * This reply need to be read by small part to determin size. Buffer
 * size is determined in function of the biggest part to read.
 *
 * The parser only requires 4 bytes.
 */
#define SOCKS5_CONN_REP_LEN       255
static gboolean
client_parse_connect_reply (const guint8 *data, gint *atype, GError **error)
{
  if (data[0] != SOCKS5_VERSION) {
    g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                         "The server is not a SOCKSv5 proxy server.");
    return FALSE;
  }

  switch (data[1]) {
    case SOCKS5_REP_SUCCEEDED:
      if (data[2] != SOCKS5_RESERVED) {
        g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                             "The server is not a SOCKSv5 proxy server.");
        return FALSE;
      }

      switch (data[3]) {
        case SOCKS5_ATYP_IPV4:
        case SOCKS5_ATYP_IPV6:
        case SOCKS5_ATYP_DOMAINNAME:
          *atype = data[3];
          break;

        default:
          g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                               "The SOCKSv5 proxy server uses unkown address type.");
          return FALSE;
      }
      break;

    case SOCKS5_REP_SRV_FAILURE:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                           "Internal SOCKSv5 proxy server error.");
      return FALSE;
      break;

    case SOCKS5_REP_NOT_ALLOWED:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_NOT_ALLOWED,
                           "SOCKSv5 connection not allowed by ruleset.");
      return FALSE;
      break;

    case SOCKS5_REP_TTL_EXPIRED:
    case SOCKS5_REP_HOST_UNREACH:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_HOST_UNREACHABLE,
                           "Host unreachable through SOCKSv5 server.");
      return FALSE;
      break;

    case SOCKS5_REP_NET_UNREACH:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_NETWORK_UNREACHABLE,
                           "Network unreachable through SOCKSv5 proxy.");
      return FALSE;
      break;

    case SOCKS5_REP_REFUSED:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_CONNECTION_REFUSED,
                           "Connection refused through SOCKSv5 proxy.");
      return FALSE;
      break;

    case SOCKS5_REP_CMD_NOT_SUP:
      g_set_error_literal (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                           "SOCKSv5 proxy does not support 'connect' command.");
      return FALSE;
      break;

    case SOCKS5_REP_ATYPE_NOT_SUP:
      g_set_error_literal (error, G_IO_ERROR, S5B_SOCKS5_ERROR_FAILED,
                           "SOCKSv5 proxy does not support provided address type.");
      return FALSE;
      break;

    default: /* Unknown error */
      g_set_error_literal (error, G_IO_ERROR, S5B_SOCKS5_ERROR_FAILED,
                           "Unkown SOCKSv5 proxy error.");
      return FALSE;
      break;
  }

  return TRUE;
}

static gint
server_set_connect_reply (guint8 *msg,
                          guint8 rep,
                          guint8 atype,
                          const guint8 *bndaddr,
                          guint16 bndport)
{
  guint len = 0, host_len;

  msg[len++] = SOCKS5_VERSION;
  msg[len++] = rep;
  if (rep != 0)
    return len;

  len++;
  msg[len++] = atype;
  switch (atype) {
    case SOCKS5_ATYP_IPV4:
      // bndaddr should already be in network byte order
      msg[len++] = SOCKS5_ATYP_IPV4;
      memcpy (msg + len, bndaddr, 4);
      len += 4;
      break;
    case SOCKS5_ATYP_DOMAINNAME:
      host_len = strlen((gchar* )bndaddr);
      msg[len++] = SOCKS5_ATYP_DOMAINNAME;
      msg[len++] = (guint8) host_len;
      memcpy (msg + len, bndaddr, host_len);
      len += host_len;
      break;
    case SOCKS5_ATYP_IPV6:
      msg[len++] = SOCKS5_ATYP_IPV6;
      memcpy (msg + len, bndaddr, 16);
      len += 16;
      break;
    default:
      return 0;
  }

  {
    guint16 hp = g_htons (bndport);
    memcpy (msg + len, &hp, 2);
    len += 2;
  }

  return len;
}

typedef struct
{
  GSimpleAsyncResult *simple;
  GIOStream *io_stream;
  GInetSocketAddress *external_address;
  gchar *hostname;
  //guint16 port; // port is always 0
  gchar *username;
  gchar *password;
  guint8 *buffer;
  gssize length;
  gssize offset;
  GCancellable *cancellable;
} ConnectAsyncData;

static void nego_msg_write_cb              (GObject          *source,
                                            GAsyncResult     *res,
                                            gpointer          user_data);
static void nego_msg_read_cb               (GObject          *source,
                                            GAsyncResult     *res,
                                            gpointer          user_data);
static void no_supported_method_cb         (GObject      *source,
                                            GAsyncResult *res,
                                            gpointer      user_data);
static void nego_reply_read_cb             (GObject          *source,
                                            GAsyncResult     *res,
                                            gpointer          user_data);
static void nego_reply_write_cb            (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      user_data);
static void auth_msg_write_cb              (GObject          *source,
                                           GAsyncResult     *res,
                                            gpointer          user_data);
static void auth_reply_read_cb             (GObject          *source,
                                            GAsyncResult     *result,
                                            gpointer          user_data);
static void send_connect_msg               (ConnectAsyncData *data);
static void connect_msg_write_cb           (GObject          *source,
                                            GAsyncResult     *result,
                                            gpointer          user_data);
static void connect_msg_read_cb            (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      user_data);
static void connect_addr_len_read_cb       (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      user_data);
static void connect_addr_read_cb           (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      user_data);
static void connect_reply_write_cb         (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      user_data);
static void connect_reply_read_cb          (GObject          *source,
                                            GAsyncResult     *result,
                                            gpointer          user_data);
static void connect_reply_addr_len_read_cb (GObject          *source,
                                            GAsyncResult     *result,
                                            gpointer          user_data);
static void connect_reply_addr_read_cb     (GObject          *source,
                                            GAsyncResult     *result,
                                            gpointer          user_data);

static void
free_connect_data (ConnectAsyncData *data)
{
  if (data->io_stream)
    g_object_unref (data->io_stream);

  if (data->external_address)
    g_object_unref(data->external_address);

  g_free (data->hostname);
  g_free (data->username);
  g_free (data->password);
  
  if (data->cancellable)
    g_object_unref (data->cancellable);

  g_slice_free (ConnectAsyncData, data);
}

static void
complete_async_from_error (ConnectAsyncData *data, GError *error)
{
  GSimpleAsyncResult *simple = data->simple;
  g_simple_async_result_set_from_error (data->simple,
                                        error);
  g_error_free (error);
  g_simple_async_result_set_op_res_gpointer (simple, NULL, NULL);
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
do_read (GAsyncReadyCallback callback, ConnectAsyncData *data)
{
   GInputStream *in;
   in = g_io_stream_get_input_stream (data->io_stream);
   g_input_stream_read_async (in,
                              data->buffer + data->offset,
                              data->length - data->offset,
                              G_PRIORITY_DEFAULT, data->cancellable,
                              callback, data);
}

static void
do_write (GAsyncReadyCallback callback, ConnectAsyncData *data)
{
  GOutputStream *out;
  out = g_io_stream_get_output_stream (data->io_stream);
  g_output_stream_write_async (out,
                               data->buffer + data->offset,
                               data->length - data->offset,
                               G_PRIORITY_DEFAULT, data->cancellable,
                               callback, data);
}

/**
 * @param hostname  with SOCKS5 Bytestreams, the hostname (dst.addr) actually
 *                  contains SHA1 Hash of: (SID + Requester JID + Target JID)
 * 
 * Function called when we act as a client and want to negociate
 * with a SOCKS5 server.
 */
void
socks5_client_nego (GIOStream            *io_stream,
                    gchar                *hostname,
                    GCancellable         *cancellable,
                    GAsyncReadyCallback   callback,
                    gpointer              user_data)
{
  GSimpleAsyncResult *simple;
  ConnectAsyncData *data;

  simple = g_simple_async_result_new (NULL,
                                      callback, user_data,
                                      socks5_client_nego);

  data = g_slice_new0 (ConnectAsyncData);

  data->simple = simple;
  data->io_stream = g_object_ref (io_stream);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);

  data->hostname = g_strdup(hostname);

  g_simple_async_result_set_op_res_gpointer (simple, data, 
                                             (GDestroyNotify) free_connect_data);

  data->buffer = g_malloc0 (SOCKS5_NEGO_MSG_LEN);
  data->length = client_set_nego_msg (data->buffer,
                                      data->username || data->password);
  data->offset = 0;

  do_write (nego_msg_write_cb, data);
}

/**
 * Function called when we act as a server and want to negociate
 * with a client.
 */
#define SOCKS5_NEGO_MSG_MAX_LEN       257 // 1 + 1 + 255
void
socks5_server_nego (GIOStream            *io_stream,
                    gchar                *allowed_hostname,
                    GInetSocketAddress   *external_address,
                    GCancellable         *cancellable,
                    GAsyncReadyCallback   callback,
                    gpointer              user_data)
{
  GSimpleAsyncResult *simple;
  ConnectAsyncData *data;

  simple = g_simple_async_result_new (NULL,
                                      callback, user_data,
                                      socks5_server_nego);

  data = g_slice_new0 (ConnectAsyncData);
  data->simple = simple;
  data->io_stream = g_object_ref (io_stream);
  data->external_address = g_object_ref (external_address);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);

  data->hostname = g_strdup(allowed_hostname);

  g_simple_async_result_set_op_res_gpointer (simple, data, 
                                             (GDestroyNotify) free_connect_data);

  data->buffer = g_malloc0 (SOCKS5_NEGO_MSG_LEN);
  data->length = SOCKS5_NEGO_MSG_MAX_LEN;
  data->offset = 0;

  do_read (nego_msg_read_cb, data);
}


static void
nego_msg_write_cb (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          res, &error);

  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    g_free (data->buffer);

    data->buffer = g_malloc0 (SOCKS5_NEGO_REP_LEN);
    data->length = SOCKS5_NEGO_REP_LEN;
    data->offset = 0;

    do_read (nego_reply_read_cb, data);
  } else {
    do_write (nego_msg_write_cb, data);
  }
}

static void
nego_msg_read_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     res, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  /* if the client already send at least NMETHODS, no need to
   * read until data->length, which is the max size */
  if ((data->offset >= 2 && data->offset-2 > data->buffer[2]) ||
      data->offset == data->length) {
    GError *error;

    if (!server_parse_nego_init (data->buffer, data->offset, &error)) {
      if (g_error_matches (error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_AUTH_FAILED)) {
        /* "If the selected METHOD is X'FF', none of the methods listed by the
           client are acceptable, and the client MUST close the connection." */
        g_free (data->buffer);
        data->buffer = g_malloc0(SOCKS5_NEGO_REP_LEN);
        data->length = server_set_nego_reply_msg(data->buffer, SOCKS5_AUTH_NO_ACCEPT);
        data->offset = 0;
        do_write (no_supported_method_cb, data);
        return;
	  }
      complete_async_from_error (data, error);
      return;
	}

    g_free (data->buffer);
    data->buffer = g_malloc0 (SOCKS5_NEGO_REP_LEN);
    data->length = server_set_nego_reply_msg (data->buffer, SOCKS5_AUTH_NONE);
    data->offset = 0;

    do_write (nego_reply_write_cb, data);
  } else {
    do_read (nego_msg_read_cb, data);
  }
}

static void
no_supported_method_cb (GObject      *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          res, &error);

  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    GError *error;
    error = g_error_new_literal (S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_AUTH_FAILED,
                                 "The client did not offer any authentication "
                                  "method we support.");
    complete_async_from_error (data, error);
  } else {
    do_write(no_supported_method_cb, data);
  }
}

static void
nego_reply_read_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     res, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;
  
  if (data->offset == data->length) {
    GError *error;
    gboolean must_auth = FALSE;
    gboolean has_auth = data->username || data->password;

    if (!client_parse_nego_reply (data->buffer, has_auth, &must_auth, &error)) {
      complete_async_from_error (data, error);
      return;
    }

    if (must_auth) {
      g_free (data->buffer);

      data->buffer = g_malloc0 (SOCKS5_AUTH_MSG_LEN);
      data->length = set_auth_msg (data->buffer,
                                   data->username,
                                   data->password,
                                   &error);
      data->offset = 0;

      if (data->length < 0) {
        complete_async_from_error (data, error);
        return;
      }
          
      do_write (auth_msg_write_cb, data);
    } else {
      send_connect_msg (data);
    }
  } else {
    do_read (nego_reply_read_cb, data);
  }
}

static void
nego_reply_write_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          res, &error);

  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    g_free (data->buffer);

    data->buffer = g_malloc0 (SOCKS5_CONN_MSG_LEN);
    data->length = 4;
    data->offset = 0;

    do_read (connect_msg_read_cb, data);
  } else {
    do_write (nego_reply_write_cb, data);
  }
}

static void
auth_msg_write_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          result, &error);
  
  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    g_free (data->buffer);

    data->buffer = g_malloc0 (SOCKS5_NEGO_REP_LEN);
    data->length = SOCKS5_NEGO_REP_LEN;
    data->offset = 0;

    do_read (auth_reply_read_cb, data);
  } else {
    do_write (auth_msg_write_cb, data);
  }
}

static void
auth_reply_read_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  if (data->offset == data->length) {
    if (!check_auth_status (data->buffer, &error)) {
      complete_async_from_error (data, error);
      return;
    }

    send_connect_msg (data);
  } else {
    do_read (auth_reply_read_cb, data);
  }
}

static void
send_connect_msg (ConnectAsyncData *data)
{
  GError *error = NULL;

  g_free (data->buffer);

  data->buffer = g_malloc0 (SOCKS5_CONN_MSG_LEN);
  data->length = client_set_connect_msg (data->buffer,
                                         data->hostname,
                                         S5B_DST_PORT,
                                         &error);
  data->offset = 0;
  
  if (data->length < 0) {
    complete_async_from_error (data, error);
    return;
  }

  do_write (connect_msg_write_cb, data);
}

static void
connect_msg_write_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          result, &error);
  
  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    g_free (data->buffer);

    data->buffer = g_malloc0 (SOCKS5_CONN_REP_LEN);
    data->length = 4;
    data->offset = 0;

    do_read (connect_reply_read_cb, data);
  } else {
    do_write (connect_msg_write_cb, data);
  }
}

static void
connect_msg_read_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  if (data->offset == data->length) {
    gint atype;

    if (!server_parse_connect_msg (data->buffer, &atype, &error)) {
      complete_async_from_error (data, error);
      return;
    }

    switch (atype) {
      case SOCKS5_ATYP_DOMAINNAME:
        data->length = 1;
        data->offset = 0;
        do_read (connect_addr_len_read_cb, data);
        break;
      default:
        g_set_error_literal (&error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_FAILED,
                             "This SOCKSv5 server implementation only support "
                             "DOMAINNAME addresses.");
        complete_async_from_error (data, error);
        return;
    }
  } else {
    do_read (connect_msg_read_cb, data);
  }
}

static void
connect_addr_len_read_cb (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->length = data->buffer[0] + 2;
  data->offset = 0;

  do_read (connect_addr_read_cb, data);
}

static void
connect_addr_read_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  if (data->offset == data->length) {
    gchar *hostname = g_strndup ((gchar *)data->buffer, data->length-2);
    if (!g_strcmp0 (data->hostname, hostname)) {
      guint8 atype;
      GInetAddress *address = g_inet_socket_address_get_address (data->external_address);
      g_free(data->buffer);
      if (g_inet_address_get_family (address) == G_SOCKET_FAMILY_IPV4)
        atype = SOCKS5_ATYP_IPV4;
      else
        atype = SOCKS5_ATYP_IPV6;

      data->buffer = g_malloc0 (SOCKS5_CONN_REP_LEN);
      data->length = server_set_connect_reply (data->buffer, SOCKS5_REP_SUCCEEDED,
                                               atype, g_inet_address_to_bytes(address),
                                               g_inet_socket_address_get_port(data->external_address));
      data->offset = 0;

      do_write (connect_reply_write_cb, data);
	} else {
      // wrong hostname ! we should respond: X'02' connection not allowed by ruleset
      g_set_error_literal (&error, S5B_SOCKS5_ERROR, S5B_SOCKS5_ERROR_NOT_ALLOWED,
                           "The client gave an invalid hostname");
      complete_async_from_error (data, error);
	}
	g_free (hostname);
  } else {
    do_read (connect_reply_read_cb, data);
  }
}

static void
connect_reply_write_cb (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
                                          result, &error);
  
  if (written < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += written;

  if (data->offset == data->length) {
    GSimpleAsyncResult *simple = data->simple;
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
  } else {
    do_write (connect_reply_write_cb, data);
  }
}

static void
connect_reply_read_cb (GObject       *source,
                       GAsyncResult  *result,
                       gpointer       user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  if (data->offset == data->length) {
    gint atype;

    if (!client_parse_connect_reply (data->buffer, &atype, &error)) {
      complete_async_from_error (data, error);
      return;
    }

    switch (atype) {
      case SOCKS5_ATYP_IPV4:
        data->length = 6;
        data->offset = 0;
        do_read (connect_reply_addr_read_cb, data);
            break;

      case SOCKS5_ATYP_IPV6:
        data->length = 18;
        data->offset = 0;
        do_read (connect_reply_addr_read_cb, data);
        break;

      case SOCKS5_ATYP_DOMAINNAME:
        data->length = 1;
        data->offset = 0;
        do_read (connect_reply_addr_len_read_cb, data);
        break;
    }
  } else {
    do_read (connect_reply_read_cb, data);
  }
}

static void
connect_reply_addr_len_read_cb (GObject      *source,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->length = data->buffer[0] + 2;
  data->offset = 0;

  do_read (connect_addr_read_cb, data);
}

static void
connect_reply_addr_read_cb (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize read;

  read = g_input_stream_read_finish (G_INPUT_STREAM (source),
                                     result, &error);

  if (read < 0) {
    complete_async_from_error (data, error);
    return;
  }

  data->offset += read;

  if (data->offset == data->length) {
    GSimpleAsyncResult *simple = data->simple;
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
  } else {
    do_read (connect_reply_read_cb, data);
  }
}

GIOStream *
g_socks5_proxy_connect_finish (GAsyncResult *result,
                               GError      **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  ConnectAsyncData *data = g_simple_async_result_get_op_res_gpointer (simple);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  return g_object_ref (data->io_stream);
}

GQuark s5b_proxy_error_quark(void)
{
  return g_quark_from_string("S5B_SOCKS5_ERROR");
}
