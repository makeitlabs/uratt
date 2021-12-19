#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "net_certs.h"
#include "esp_log.h"

static const char *TAG = "https";

static SemaphoreHandle_t s_fd_mutex;
static int s_fd;

esp_err_t http_init(void)
{
    s_fd = 0;
    s_fd_mutex = xSemaphoreCreateMutex();

    return ESP_OK;
}

esp_err_t http_get_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            ESP_LOGD(TAG, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int r = write(s_fd, evt->data, evt->data_len);
                if (r < 0) {
                  ESP_LOGE(TAG, "error writing %d bytes to file", evt->data_len);
                }
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}


esp_err_t http_get_file(int id, const char *url, const char *auth_user, const char *auth_pass, const char *filename, int* result)
{
  xSemaphoreTake(s_fd_mutex, portMAX_DELAY);
  s_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC);

  if (s_fd < 0) {
      ESP_LOGE(TAG, "can't open file %s for write", filename);
      xSemaphoreGive(s_fd_mutex);
      return ESP_FAIL;
  }

  esp_http_client_config_t config = {
     .url = url,
     .auth_type = HTTP_AUTH_TYPE_BASIC,
     .username = auth_user,
     .password = auth_pass,
     //.crt_bundle_attach = esp_crt_bundle_attach,

     .client_cert_pem = g_client_cert,
     .client_key_pem = g_client_key,
     .cert_pem = g_ca_cert,
     .skip_cert_common_name_check = true,

     .event_handler = http_get_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGD(TAG, "Status = %d, content_length = %lld",
      esp_http_client_get_status_code(client),
      esp_http_client_get_content_length(client));

    *result = esp_http_client_get_status_code(client);
  }
  esp_http_client_cleanup(client);

  close(s_fd);
  xSemaphoreGive(s_fd_mutex);
  return err;
}



esp_err_t http_post(int id, const char *url, const char *data, char *response, int size)
{
  return ESP_FAIL;
}
