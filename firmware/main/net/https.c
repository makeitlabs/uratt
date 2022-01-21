#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include "mbedtls/md.h"

#include "https.h"

#include "esp_task_wdt.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

static const char *TAG = "https";

static SemaphoreHandle_t s_busy_mutex;
static int s_fd = 0;
static mbedtls_md_context_t s_md_ctx;

static http_get_req_t* s_req;

esp_err_t http_init(void)
{
    s_busy_mutex = xSemaphoreCreateMutex();

    return ESP_OK;
}


static esp_err_t http_get_file_event_handler(esp_http_client_event_t *evt)
{
    static size_t content_length = 0;
    static size_t received_length = 0;

    esp_task_wdt_reset();

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");

            content_length = 0;
            received_length = 0;

            if (s_req->resp_hash_buf) {
              mbedtls_md_init(&s_md_ctx);
              if (mbedtls_md_setup(&s_md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA224), 0) != ESP_OK ||
                  mbedtls_md_starts(&s_md_ctx) != ESP_OK) {
                  ESP_LOGE(TAG, "error setting up mbedtls");
              }
            }
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER %s=%s", evt->header_key, evt->header_value);

            if (strcmp(evt->header_key, "Content-Length")==0) {
              content_length = atoi(evt->header_value);
            } else if (strcmp(evt->header_key, "X-Hash-SHA224")==0) {

              if (s_req->hash_expected && strcmp(evt->header_value, s_req->hash_expected)==0) {
                if (s_req->hash_expected_cancel) {
                  ESP_LOGW(TAG, "X-Hash-SHA224 matches expected hash %s, not downloading.", s_req->hash_expected);

                  // set a short timeout so it closes immediately
                  esp_http_client_set_timeout_ms(evt->client, 10);
                  esp_http_client_close(evt->client);
                  s_req->resp_hash_expected_match = true;

                  if (s_req->resp_hash_buf) {
                    strncpy(s_req->resp_hash_buf, evt->header_value, (224/8));
                  }

                  if (s_req->progress_cb) {
                    s_req->progress_cb(1, 1); // 100%
                  }
                }
              }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGD(TAG, "Receive %d bytes", evt->data_len);
                received_length += evt->data_len;

                if (s_req->progress_cb) {
                  s_req->progress_cb(received_length, content_length);
                }

                if (s_req->resp_hash_buf) {
                  if (mbedtls_md_update(&s_md_ctx, evt->data, evt->data_len) != ESP_OK) {
                    ESP_LOGE(TAG, "error computing sha224 on %d bytes of data", evt->data_len);
                  }
                }

                int r = write(s_fd, evt->data, evt->data_len);
                if (r < 0) {
                  ESP_LOGE(TAG, "error writing %d bytes to file", evt->data_len);
                }
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (s_req->resp_hash_buf) {
              uint8_t hbuf[32];
              mbedtls_md_finish(&s_md_ctx, hbuf);
              mbedtls_md_free(&s_md_ctx);

              if (!s_req->resp_hash_expected_match) {
                for (uint8_t idx=0; idx<(224/8); idx++) {
                  sprintf(s_req->resp_hash_buf + (idx * 2), "%2.2x", hbuf[idx]);
                }
              }

            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}


esp_err_t http_get(http_get_req_t* req)
{
  xSemaphoreTake(s_busy_mutex, portMAX_DELAY);

  s_req = req;

  esp_http_client_config_t config = {
     .url = req->url
  };

  if (req->filename) {
    s_fd = open(req->filename, O_WRONLY|O_CREAT|O_TRUNC);

    if (s_fd < 0) {
        ESP_LOGE(TAG, "can't open file %s for write", req->filename);
        xSemaphoreGive(s_busy_mutex);
        return ESP_FAIL;
    }

    config.event_handler = http_get_file_event_handler;
  }

  if (req->auth_user != NULL && req->auth_password != NULL) {
    config.auth_type =  HTTP_AUTH_TYPE_BASIC;
    config.username = req->auth_user;
    config.password = req->auth_password;
  } else {
    config.auth_type =  HTTP_AUTH_TYPE_NONE;
  }

  if (req->client_cert_pem != NULL && req->client_key_pem != NULL && req->ca_cert_pem != NULL) {
    config.client_cert_pem = req->client_cert_pem;
    config.client_key_pem = req->client_key_pem;
    config.cert_pem = req->ca_cert_pem;
    config.skip_cert_common_name_check = req->ssl_insecure;
  } else {
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    req->resp_status = esp_http_client_get_status_code(client);
    req->resp_content_length = esp_http_client_get_content_length(client);

    ESP_LOGD(TAG, "Status = %d, content_length = %u", req->resp_status, req->resp_content_length);
    if (!req->resp_hash_expected_match && req->resp_hash_buf) {
      ESP_LOGI(TAG, "Calculated SHA224 hash %s", req->resp_hash_buf);
    }

  }
  esp_http_client_cleanup(client);

  if (req->filename) {
    close(s_fd);
  }
  xSemaphoreGive(s_busy_mutex);
  return err;
}
