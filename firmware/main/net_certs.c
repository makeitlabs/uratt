#include "esp_log.h"
#include "esp_system.h"
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "net_certs.h"

char* g_client_cert = NULL;
char* g_client_key = NULL;
char* g_ca_cert = NULL;

static const char *TAG = "net_certs";

esp_err_t net_certs_load(char* filename, char** buf)
{
  struct stat st;

  if (stat(filename, &st) == -1) {
    ESP_LOGE(TAG, "can't stat %s", filename);
    return -1;
  }

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    ESP_LOGE(TAG, "can't open %s", filename);
    return -1;
  }

  *buf = malloc(st.st_size);
  if (*buf == NULL) {
    ESP_LOGE(TAG, "can't malloc %ld bytes to load %s", (long)st.st_size, filename);
    close(fd);
    return -1;
  }

  int r = read(fd, *buf, 1);
  if (r != st.st_size) {
    ESP_LOGE(TAG, "couldn't read %ld bytes from %s", (long)st.st_size, filename);
    free(*buf);
    close(fd);
    return -1;
  }

  close(fd);
  ESP_LOGI(TAG, "Loaded certificate (%ld bytes) from %s", (long)st.st_size, filename);
  return ESP_OK;
}

esp_err_t net_certs_init()
{
  if (net_certs_load("/certs/ca.crt", &g_ca_cert) != ESP_OK) {
    g_ca_cert = NULL;
  }

  if (net_certs_load("/certs/client.crt", &g_client_cert) != ESP_OK) {
    g_client_cert = NULL;
  }

  if (net_certs_load("/certs/client.key", &g_client_key) != ESP_OK) {
    g_client_key = NULL;
  }

  if (g_ca_cert == NULL || g_client_cert == NULL || g_client_key == NULL) {
    ESP_LOGE(TAG, "One or more certificates failed to load!");
    return -1;
  }

  return ESP_OK;
}
