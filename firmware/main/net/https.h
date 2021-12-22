#ifndef _HTTPS_H
#define _HTTPS_H

esp_err_t http_init(void);

typedef struct {
  const char *url;
  const char *auth_user;        // if no user/pass specified, no auth used
  const char *auth_password;
  const char *client_cert_pem;  // if client cert/key and ca/cert not specified, standard cert bundle used
  const char *client_key_pem;
  const char *ca_cert_pem;
  bool ssl_insecure;            // ignore CN in cert

  void (*progress_cb)(int,int);

  const char *filename;         // if NULL then you must allocate a buffer and pass it in below
  char *data_buf;
  size_t data_buf_len;

  int status;
  size_t content_length;
  char *hash_buf;         // MUST be at least 57 bytes; if NULL no hash will be returned
} http_get_req_t;

esp_err_t http_get(http_get_req_t* req);

#endif
