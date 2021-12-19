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

static bool s_initialized = false;

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


void net_sntp_init(void)
{
  if (!s_initialized) {
    static char ntp_server[64];
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Initializing SNTP...");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    char *conf_ntp_server;
    config_get_string("ntp_server", &conf_ntp_server, "pool.ntp.org");
    strncpy(ntp_server, conf_ntp_server, sizeof(ntp_server));
    free(conf_ntp_server);
    ESP_LOGI(TAG, "ntp server is %s", ntp_server);

    sntp_setservername(0, ntp_server);

    sntp_set_time_sync_notification_cb(net_sntp_sync_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

    sntp_set_sync_interval(1800000);

    ESP_LOGI(TAG, "ntp update interval is %d msec", sntp_get_sync_interval());
    sntp_init();

    s_initialized = true;
  }
}

void net_sntp_stop(void)
{
  ESP_LOGI(TAG, "Stopping SNTP...");
  sntp_stop();
  s_initialized = false;
}
