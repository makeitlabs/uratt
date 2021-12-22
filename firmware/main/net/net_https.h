#ifndef _NET_HTTPS
#define _NET_HTTPS

int net_https_init();
esp_err_t net_https_download_acl();

int net_https_get_file(const char *web_url, const char *filename);


#endif
