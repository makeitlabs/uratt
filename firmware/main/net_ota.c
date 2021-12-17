#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include "net_ota.h"
#include "net_certs.h"
#include "esp_wifi.h"

static const char *TAG = "ota_task";

#define OTA_URL_SIZE 256

void net_ota_init(void)
{

}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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
    ESP_LOGI(TAG, "Starting OTA Update...");

    esp_http_client_config_t config = {
        .url = CONFIG_FIRMWARE_UPGRADE_URL,
        .cert_pem = g_client_cert,
        .event_handler = _http_event_handler,
    };

    char *conf_ota_url;
    config_get_string("ota_url", &conf_ota_url, "https://my-server.org/ota.bin");
    config.url = conf_ota_url;

#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_err_t ret = esp_https_ota(&config);

    free(conf_ota_url);

    if (ret == ESP_OK) {
        esp_restart();
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
}
