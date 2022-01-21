#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_task_wdt.h"
#include "string.h"
#include "net_ota.h"
#include "net_certs.h"
#include "net_mqtt.h"
#include "esp_wifi.h"
#include "config.h"
#include "main_task.h"
#include "display_task.h"

static const char *TAG = "ota";

#define OTA_URL_SIZE 256

void net_ota_init(void)
{
}

void net_ota_progress(size_t received, size_t total)
{
  static int last_percent = -1;
  int percent = received * 100 / total;

  if (percent != last_percent) {
    display_ota_status(OTA_STATUS_DOWNLOADING, percent);
    net_mqtt_send_ota_status(OTA_STATUS_DOWNLOADING, percent);
  }

  last_percent = percent;
}


esp_err_t net_ota_http_event_handler(esp_http_client_event_t *evt)
{
    static size_t content_length = 0;
    static size_t received_length = 0;

    esp_task_wdt_reset();

    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        content_length = 0;
        received_length = 0;
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key, "Content-Length")==0) {
          content_length = atoi(evt->header_value);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        received_length += evt->data_len;
        net_ota_progress(received_length, content_length);
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


void net_ota_update(void)
{
    esp_http_client_config_t config = {
        .cert_pem = g_ca_cert,
        .skip_cert_common_name_check = true,
        .keep_alive_enable = true,
        .event_handler = net_ota_http_event_handler,
    };

    char *conf_ota_url;
    config_get_string("ota_url", &conf_ota_url, "https://my-server.org/ota.bin");
    config.url = conf_ota_url;

    ESP_LOGI(TAG, "Starting OTA update from %s", conf_ota_url);


    display_ota_status(OTA_STATUS_DOWNLOADING, 0);
    net_mqtt_send_ota_status(OTA_STATUS_DOWNLOADING, 0);

    esp_err_t ret = esp_https_ota(&config);

    free(conf_ota_url);

    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "OTA firmware download success");
        net_mqtt_send_ota_status(OTA_STATUS_APPLYING, 100);
        vTaskDelay(500/portTICK_PERIOD_MS);
        main_task_event(MAIN_EVT_OTA_UPDATE_SUCCESS);
    } else {
        ESP_LOGE(TAG, "OTA firmware upgrade failed ret=%d", ret);
        net_mqtt_send_ota_status(OTA_STATUS_ERROR, 0);
        display_ota_status(OTA_STATUS_ERROR, 100);
        main_task_event(MAIN_EVT_OTA_UPDATE_FAILED);
    }
}
