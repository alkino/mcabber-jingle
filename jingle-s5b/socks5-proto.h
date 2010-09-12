#ifndef __SOCKS5_PROTO_H__
#define __SOCKS5_PROTO_H__ 1

#define S5B_SOCKS5_ERROR s5b_proxy_error_quark()


typedef enum {
  S5B_SOCKS5_ERROR_HOST_UNREACHABLE,
  S5B_SOCKS5_ERROR_NETWORK_UNREACHABLE,
  S5B_SOCKS5_ERROR_CONNECTION_REFUSED,
  S5B_SOCKS5_ERROR_FAILED,
  S5B_SOCKS5_ERROR_AUTH_FAILED,
  S5B_SOCKS5_ERROR_NEED_AUTH,
  S5B_SOCKS5_ERROR_NOT_ALLOWED
} S5bSocks5Error;

void
socks5_nego_with_server (GIOStream            *io_stream,
                         gchar                *hostname,
                         GCancellable         *cancellable,
                         GAsyncReadyCallback   callback,
                         gpointer              user_data);

GIOStream *
g_socks5_proxy_connect_finish (GAsyncResult *result,
                               GError      **error);

GQuark s5b_proxy_error_quark(void);

#endif
