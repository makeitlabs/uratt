#ifndef _HTTPS_H
#define _HTTPS_H

esp_err_t http_init(void);

esp_err_t http_get_file(int id, const char *url, const char *auth_user, const char *auth_pass, const char *filename, int* result);
esp_err_t http_post(int id, const char *url, const char *data, char *response, int size);

#endif
