/*--------------------------------------------------------------------------
  _____       ______________
 |  __ \   /\|__   ____   __|
 | |__) | /  \  | |    | |
 |  _  / / /\ \ | |    | |
 | | \ \/ ____ \| |    | |
 |_|  \_\/    \_\_|    |_|    ... RFID ALL THE THINGS!

 A resource access control and telemetry solution for Makerspaces

 Developed at MakeIt Labs - New Hampshire's First & Largest Makerspace
 http://www.makeitlabs.com/

 Copyright 2017-2020 MakeIt Labs

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 --------------------------------------------------------------------------
 Author: Steve Richardson (steve.richardson@makeitlabs.com)
 -------------------------------------------------------------------------- */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "config.h"
#include "https.h"
#include "sdcard.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "mbedtls/base64.h"

#include "display_task.h"
#include "rfid_task.h"
#include "net_certs.h"
#include "net_task.h"
#include "net_https.h"
#include "net_mqtt.h"
#include "net_sntp.h"
#include "net_ota.h"

static const char *TAG = "net_task";

void net_timer(TimerHandle_t xTimer);


#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)


static int s_active_interfaces = 0;
static xSemaphoreHandle s_semph_get_ip_addrs;
static esp_netif_t *s_example_esp_netif = NULL;



#define NET_QUEUE_DEPTH 8

typedef struct net_evt {
  uint8_t cmd;
  char* buf1;
  union {
      char* buf2;
      uint8_t allowed;
  } params;
} net_evt_t;

static QueueHandle_t m_q;

uint8_t g_mac_addr[6];
static esp_ip4_addr_t s_ip_addr;


/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

esp_netif_t *get_netif(void)
{
    return s_example_esp_netif;
}

esp_netif_t *get_netif_from_desc(const char *desc)
{
    esp_netif_t *netif = NULL;
    char *expected_desc;
    asprintf(&expected_desc, "%s: %s", TAG, desc);
    while ((netif = esp_netif_next(netif)) != NULL) {
        if (strcmp(esp_netif_get_desc(netif), expected_desc) == 0) {
            free(expected_desc);
            return netif;
        }
    }
    free(expected_desc);
    return netif;
}


static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  if (!is_our_netif(TAG, event->esp_netif)) {
      ESP_LOGW(TAG, "Got IPv4 from another interface \"%s\": ignored", esp_netif_get_desc(event->esp_netif));
      return;
  }
  ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
  memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
  xSemaphoreGive(s_semph_get_ip_addrs);

  /*
  char s[32];
  snprintf(s, sizeof(s), IPSTR, IP2STR(&event->ip_info.ip));
  display_wifi_msg(s);
  */
  display_wifi_status(WIFI_STATUS_CONNECTED);

  net_cmd_queue(NET_CMD_INIT);
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");

    display_wifi_status(WIFI_STATUS_DISCONNECTED);


    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        display_wifi_status(WIFI_STATUS_ERROR);
        return;
    }

    display_wifi_status(WIFI_STATUS_CONNECTING);

    ESP_ERROR_CHECK(err);
}

static esp_netif_t *net_wifi_start(void)
{
    char *desc;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();

    // Prefix the interface description with the module TAG
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    asprintf(&desc, "%s: %s", TAG, esp_netif_config.if_desc);
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    free(desc);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
        },
    };

    char *conf_ssid;
    char *conf_password;
    config_get_string("wifi_ssid", &conf_ssid, "ssid");
    config_get_string("wifi_password", &conf_password, "password");
    strncpy((char*)wifi_config.sta.ssid, conf_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, conf_password, sizeof(wifi_config.sta.password));
    free(conf_ssid);
    free(conf_password);

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    display_wifi_status(WIFI_STATUS_CONNECTING);
    esp_wifi_connect();
    return netif;
}

static void net_wifi_stop(void)
{
    esp_netif_t *wifi_netif = get_netif_from_desc("sta");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);
    s_example_esp_netif = NULL;

    display_wifi_status(WIFI_STATUS_DISCONNECTED);
}




static void net_start(void)
{
    s_example_esp_netif = net_wifi_start();
    s_active_interfaces++;
    /* create semaphore if at least one interface is active */
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0);
}

static void net_stop(void)
{
    net_wifi_stop();
    s_active_interfaces--;
}


esp_err_t net_connect(void)
{
    if (s_semph_get_ip_addrs != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    net_start();
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&net_stop));
    ESP_LOGI(TAG, "Waiting for IP(s)");
    for (int i = 0; i < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++i) {
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
    }
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t *netif = NULL;
    esp_netif_ip_info_t ip;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        netif = esp_netif_next(netif);
        if (is_our_netif(TAG, netif)) {
            ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR, IP2STR(&ip.ip));
        }
    }
    return ESP_OK;
}

esp_err_t net_disconnect(void)
{
    if (s_semph_get_ip_addrs == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    vSemaphoreDelete(s_semph_get_ip_addrs);
    s_semph_get_ip_addrs = NULL;
    net_stop();
    ESP_ERROR_CHECK(esp_unregister_shutdown_handler(&net_stop));
    return ESP_OK;
}

esp_err_t net_cmd_queue(int cmd)
{
    net_evt_t evt;
    evt.cmd = cmd;
    evt.buf1 = NULL;
    evt.params.buf2 = NULL;
    return (xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

esp_err_t net_cmd_queue_access(char *member, int allowed)
{
    net_evt_t evt;
    evt.cmd = NET_CMD_SEND_ACCESS;
    evt.buf1 = malloc(strlen(member) + 1);
    if (evt.buf1) {
      strncpy(evt.buf1, member, strlen(member) + 1);
      evt.params.buf2 = NULL;
      evt.params.allowed = allowed;
      return (xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS) == pdTRUE) ? ESP_OK : ESP_FAIL;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t net_cmd_queue_access_error(char *err, char *err_ext)
{
    net_evt_t evt;
    evt.cmd = NET_CMD_SEND_ACCESS_ERROR;
    evt.buf1 = malloc(strlen(err) + 1);
    evt.params.buf2 = malloc(strlen(err_ext) + 1);
    if (evt.buf1 && evt.params.buf2) {
      strncpy(evt.buf1, err, strlen(err) + 1);
      strncpy(evt.params.buf2, err, strlen(err_ext) + 1);
      return (xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS) == pdTRUE) ? ESP_OK : ESP_FAIL;
    }
    free(evt.buf1);
    free(evt.params.buf2);
    return ESP_ERR_NO_MEM;
}

esp_err_t net_cmd_queue_wget(char *url, char *filename)
{
    net_evt_t evt;
    evt.cmd = NET_CMD_WGET;
    evt.buf1 = malloc(strlen(url) + 1);
    evt.params.buf2 = malloc(strlen(filename) + 1);
    if (evt.buf1 && evt.params.buf2) {
      strncpy(evt.buf1, url, strlen(url) + 1);
      strncpy(evt.params.buf2, filename, strlen(filename) + 1);
      return (xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS) == pdTRUE) ? ESP_OK : ESP_FAIL;
    }
    free(evt.buf1);
    free(evt.params.buf2);
    return ESP_ERR_NO_MEM;
}


void net_init(void)
{
    ESP_LOGI(TAG, "Initializing network task...");

    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("wifi_init", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);

    display_wifi_status(WIFI_STATUS_INIT);

    m_q = xQueueCreate(NET_QUEUE_DEPTH, sizeof(net_evt_t));
    if (m_q == NULL) {
        ESP_LOGE(TAG, "FATAL: Cannot create net queue!");
    }

    // get MAC address from efuse
    esp_efuse_mac_get_default(g_mac_addr);
    ESP_LOGI(TAG, "My mac adddress is %2x%2x%2x%2x%2x%2x", g_mac_addr[0],g_mac_addr[1],g_mac_addr[2],g_mac_addr[3],g_mac_addr[4],g_mac_addr[5]);

    net_certs_init();
    net_https_init();
    net_mqtt_init();
    net_ota_init();

    TimerHandle_t timer = xTimerCreate("rssi_timer", (1000 / portTICK_PERIOD_MS), pdTRUE, (void*) 0, net_timer);
    if (xTimerStart(timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Could not start net timer");
    }

}


void net_task(void *pvParameters)
{
    ESP_LOGI(TAG, "start net task");
    net_init();

    while(1) {
      net_evt_t evt;

      if (xQueueReceive(m_q, &evt, (20 / portTICK_PERIOD_MS)) == pdPASS) {
        switch(evt.cmd) {
          case NET_CMD_INIT:
            net_sntp_init();
            net_cmd_queue(NET_CMD_DOWNLOAD_ACL);
            //net_mqtt_start();
            break;

          case NET_CMD_DISCONNECT:
            net_sntp_stop();
            net_mqtt_stop();
            net_disconnect();
            break;

          case NET_CMD_CONNECT:
            net_connect();
            break;

          case NET_CMD_DOWNLOAD_ACL:
            {
              display_acl_status(ACL_STATUS_DOWNLOADING);
              net_mqtt_stop();

              time_t start = esp_log_timestamp();
              esp_err_t r = net_https_download_acl();
              time_t elapsed = esp_log_timestamp() - start;
              if (r == ESP_OK) {
                ESP_LOGI(TAG, "ACL download OK, took %ld.%ld seconds", elapsed / 1000, elapsed % 1000);
                display_acl_status(ACL_STATUS_DOWNLOADED_UPDATED);
              } else {
                ESP_LOGE(TAG, "ACL download failed, took %ld.%ld seconds", elapsed / 1000, elapsed % 1000);
                display_acl_status(ACL_STATUS_ERROR);
              }
              net_mqtt_start();

            }
            break;

          case NET_CMD_SEND_ACL_UPDATED:
            net_mqtt_send_acl_updated(MQTT_ACL_SUCCESS);
            break;

          case NET_CMD_SEND_ACL_FAILED:
            net_mqtt_send_acl_updated(MQTT_ACL_FAIL);
            break;

          case NET_CMD_SEND_WIFI_STR:
            net_mqtt_send_wifi_strength();
            break;

          case NET_CMD_SEND_ACCESS:
            net_mqtt_send_access(evt.buf1, evt.params.allowed);
            free(evt.buf1);
            break;

          case NET_CMD_SEND_ACCESS_ERROR:
            net_mqtt_send_access_error(evt.buf1, evt.params.buf2);
            free(evt.buf1);
            free(evt.params.buf2);
            break;

          case NET_CMD_OTA_UPDATE:
            net_ota_update();
            break;

          case NET_CMD_WGET:
            net_https_get_file(evt.buf1, evt.params.buf2);
            free(evt.buf1);
            free(evt.params.buf2);
            break;

          default:
            ESP_LOGE(TAG, "Unknown net event cmd %d", evt.cmd);
            break;
        }
      }
    }
}

void net_timer(TimerHandle_t xTimer)
{
    static int interval = 0;

    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata)==0){
        display_wifi_rssi(wifidata.rssi);

        if (interval % 60 == 0) {
          net_cmd_queue(NET_CMD_SEND_WIFI_STR);
        }

        interval++;
    }
}


/*
#ifdef MBEDTLS_DEBUG_C
#define MBEDTLS_DEBUG_LEVEL 4

// mbedtls debug function that translates mbedTLS debug output
//  to ESP_LOGx debug output.
//   MBEDTLS_DEBUG_LEVEL 4 means all mbedTLS debug output gets sent here,
//   and then filtered to the ESP logging mechanism.

static void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    const char *MBTAG = "mbedtls";
    char *file_sep;

    // Shorten 'file' from the whole file path to just the filename
    // This is a bit wasteful because the macros are compiled in with
    // the full _FILE_ path in each case.

    file_sep = rindex(file, '/');
    if(file_sep)
        file = file_sep+1;

    switch(level) {
    case 1:
        ESP_LOGI(MBTAG, "%s:%d %s", file, line, str);
        break;
    case 2:
    case 3:
        ESP_LOGD(MBTAG, "%s:%d %s", file, line, str);
    case 4:
        ESP_LOGV(MBTAG, "%s:%d %s", file, line, str);
        break;
    default:
        ESP_LOGE(MBTAG, "Unexpected log level %d: %s", level, str);
        break;
    }
}

#endif
*/
