/* LwIP SNTP example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "net_sntp.h"
#include "config.h"

static const char *TAG = "net_sntp";


#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL);
   ESP_LOGI(TAG, "Time is synchronized from custom code");
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

void net_sntp_sync_cb(struct timeval *tv)
{
    char strftime_buf[64];
    struct tm timeinfo;
    time_t now;

    char *conf_tz;
    config_get_string("tz", &conf_tz, "EST5EDT,M3.2.0/2,M11.1.0");
    setenv("TZ", conf_tz, 1);

    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    ESP_LOGI(TAG, "Time synced, %s time is: %s", conf_tz, strftime_buf);

    free(conf_tz);
}

void net_sntp_sync(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time not yet set, getting initial time.");
        net_sntp_obtain_time();

        // update 'now' variable with current time
        time(&now);
    }

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                        (long)outdelta.tv_sec,
                        outdelta.tv_usec/1000,
                        outdelta.tv_usec%1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}

void net_sntp_obtain_time(void)
{
    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGD(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    if (retry < retry_count) {
      char strftime_buf[64];
      time_t now = 0;
      struct tm timeinfo = { 0 };

      time(&now);
      localtime_r(&now, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
      ESP_LOGI(TAG, "Initial time synced, time is: %s", strftime_buf);

      sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    }
}

void net_sntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    char *conf_ntp_server;
    config_get_string("ntp_server", &conf_ntp_server, "pool.ntp.org");
    ESP_LOGI(TAG, "ntp server is %s", conf_ntp_server);


    sntp_setservername(0, conf_ntp_server);
    free(conf_ntp_server);

    sntp_set_time_sync_notification_cb(net_sntp_sync_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_init();
}
