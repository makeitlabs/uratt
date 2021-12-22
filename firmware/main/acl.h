#ifndef _ACL_H
#define _ACL_H

esp_err_t acl_init(void);
esp_err_t acl_get_data_filename(char **s);
esp_err_t acl_get_hash_filename(char **s);
esp_err_t acl_get_stored_hash__acl_mutex(const char* filename, char *hash);
esp_err_t acl_compute_stored_hash__acl_mutex(const char* filename, char *hash);
esp_err_t acl_validate(void);

extern const size_t sha224_len;
extern SemaphoreHandle_t g_acl_mutex;


#endif
