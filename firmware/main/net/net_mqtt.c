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
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "config.h"
#include "https.h"

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
#include "esp_ota_ops.h"

#include "mqtt_client.h"
#include "main_task.h"
#include "rfid_task.h"
#include "net_task.h"
#include "net_mqtt.h"
#include "net_certs.h"
#include "display_task.h"

static const char *TAG = "net_mqtt";

static esp_mqtt_client_handle_t s_mqtt_client;
static bool s_mqtt_connected = false;

int net_mqtt_topic_targeted(char *topic_type, char *subtopic, char *obuf, size_t obuf_len)
{
  return snprintf(obuf, obuf_len, "%s/%s/node/%02x%02x%02x%02x%02x%02x/%s",
    MQTT_BASE_TOPIC, topic_type,
    g_mac_addr[0], g_mac_addr[1], g_mac_addr[2], g_mac_addr[3], g_mac_addr[4], g_mac_addr[5],
    subtopic);
}

void net_mqtt_send_wifi_strength(void)
{
  wifi_ap_record_t wifidata;
  if (esp_wifi_sta_get_ap_info(&wifidata)==0) {
    // TOPIC=ratt/status/node/b827eb206a6c/wifi/status
    // DATA={"ap": "46:D9:E7:69:BB:67", "freq": "2.412", "quality": 60, "essid": "MakeIt Members", "level": -68}

    char *topic, *payload;
    topic = malloc(128);
    payload = malloc(256);

    net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "wifi/status", topic, 128);

    const int chan_freq[] = { 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484 };

    snprintf(payload, 256, "{\"ap\": \"%02X:%02X:%02X:%02X:%02X:%02X\", \"freq\": \"%1d.%3d\", \"essid\": \"%s\", \"level\": %d}",
      wifidata.bssid[0],wifidata.bssid[1],wifidata.bssid[2],wifidata.bssid[3],wifidata.bssid[4],wifidata.bssid[5],
      (wifidata.primary <= 16) ? chan_freq[wifidata.primary-1] / 1000 : 0, (wifidata.primary <= 16) ? chan_freq[wifidata.primary-1] % 1000 : 0,
      wifidata.ssid,
      wifidata.rssi);

    // QOS 0 - not very important.
    if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 0, 0) != -1) {
      display_mqtt_status(MQTT_STATUS_DATA_SENT);
      ESP_LOGD(TAG, "published wifi status");
    } else {
      ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
    }

    free(topic);
    free(payload);
  }
}


void net_mqtt_send_acl_updated(char* status)
{
  // ratt/status/node/b827eb2f8dca/acl/update {"status":"downloaded"}

  char *topic, *payload;
  topic = malloc(128);
  payload = malloc(128);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "acl/update", topic, 128);

  snprintf(payload, 128, "{\"status\":\"%s\"}", status);
  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published acl update status");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}


void net_mqtt_send_access(char *member, int allowed)
{
  char *topic, *payload;
  topic = malloc(128);
  payload = malloc(128);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "personality/access", topic, 128);

  snprintf(payload, 128, "{\"member\": \"%s\", \"allowed\": %s}", member, allowed ? "true" : "false");
  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published personality access");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}

void net_mqtt_send_access_error(char *err_text, char *err_ext)
{
  char *topic, *payload;
  topic = malloc(128);
  payload = malloc(512);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "personality/access", topic, 128);

  snprintf(payload, 512, "{\"error\": true, \"errorText\": \"%s\", \"errorExt\": \"%s\"}", err_text, err_ext);

  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published personality access error");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}

void net_mqtt_send_boot_status(void)
{
  char *topic, *payload;
  char reason[20];
  char fw_sha[65];
  topic = malloc(128);
  payload = malloc(256);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "system/boot", topic, 128);

  esp_reset_reason_t reset_reason = esp_reset_reason();

  switch (reset_reason) {
    case ESP_RST_POWERON:
      strncpy(reason, "power_on", sizeof(reason));
      break;
    case ESP_RST_EXT:
      strncpy(reason, "ext", sizeof(reason));
      break;
    case ESP_RST_SW:
      strncpy(reason, "sw", sizeof(reason));
      break;
    case ESP_RST_PANIC:
      strncpy(reason, "panic", sizeof(reason));
      break;
    case ESP_RST_INT_WDT:
      strncpy(reason, "int_wdt", sizeof(reason));
      break;
    case ESP_RST_TASK_WDT:
      strncpy(reason, "task_wdt", sizeof(reason));
      break;
    case ESP_RST_DEEPSLEEP:
      strncpy(reason, "deep_sleep", sizeof(reason));
      break;
    case ESP_RST_BROWNOUT:
      strncpy(reason, "brownout", sizeof(reason));
      break;
    case ESP_RST_SDIO:
      strncpy(reason, "sdio", sizeof(reason));
      break;
    case ESP_RST_UNKNOWN:
    default:
      strncpy(reason, "unknown", sizeof(reason));
      break;
  }

  const esp_app_desc_t* desc = esp_ota_get_app_description();

  strcpy(fw_sha, "");
  for (int i=0; i<32; i++) {
    char s[3];
    snprintf(s, 3, "%02x", desc->app_elf_sha256[i]);
    strcat(fw_sha, s);
  }

  snprintf(payload, 512, "{\"reset_reason\": \"%s\", \"fw_name\": \"%s\", \"fw_version\": \"%s\", \"fw_date\": \"%s\", \"fw_time\": \"%s\", \"fw_sha256\": \"%s\", \"idf_ver\": \"%s\"}",
           reason, desc->project_name, desc->version, desc->date, desc->time, fw_sha, desc->idf_ver);

  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published system boot status");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}


void net_mqtt_send_power_status(power_status_t status)
{
  char *topic, *payload;
  topic = malloc(128);
  payload = malloc(128);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "system/power", topic, 128);

  switch (status) {
    case POWER_STATUS_ON_EXT:
      snprintf(payload, 128, "{\"state\": \"on_external\"}");
      break;
    case POWER_STATUS_ON_BATT:
      snprintf(payload, 128, "{\"state\": \"on_battery\"}");
      break;
    case POWER_STATUS_ON_BATT_LOW:
      snprintf(payload, 128, "{\"state\": \"on_battery_low\"}");
      break;
    case POWER_STATUS_SLEEP:
      snprintf(payload, 128, "{\"state\": \"sleep\"}");
      break;
    case POWER_STATUS_WAKE:
      snprintf(payload, 128, "{\"state\": \"wake\"}");
      break;
    default:
      snprintf(payload, 128, "{\"state\": \"unknown\"}");
      break;
  }

  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published system power status");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}



void net_mqtt_send_door_state(bool door_open)
{
  char *topic, *payload;
  topic = malloc(128);
  payload = malloc(128);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "personality/door_state", topic, 128);

  if (door_open) {
    snprintf(payload, 128, "{\"state\": \"open\"}");
  } else {
    snprintf(payload, 128, "{\"state\": \"closed\"}");
  }

  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published personality door status");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}



void net_mqtt_send_ota_status(ota_status_t status, int progress)
{
  char *topic, *payload;
  char st[20];
  topic = malloc(128);
  payload = malloc(128);

  net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "system/ota_status", topic, 128);

  switch (status) {
    case OTA_STATUS_INIT:
      strncpy(st, "init", sizeof(st));
      break;
    case OTA_STATUS_ERROR:
      strncpy(st, "error", sizeof(st));
      break;
    case OTA_STATUS_DOWNLOADING:
      strncpy(st, "downloading", sizeof(st));
      break;
    case OTA_STATUS_APPLYING:
      strncpy(st, "applying", sizeof(st));
      break;
    default:
      strncpy(st, "unknown", sizeof(st));
      break;
  }

  snprintf(payload, 128, "{\"status\": \"%s\", \"progress\": \"%d\"}", st, progress);

  if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
    display_mqtt_status(MQTT_STATUS_DATA_SENT);
    ESP_LOGD(TAG, "published system ota status");
  } else {
    ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
  }

  free(topic);
  free(payload);
}


static esp_err_t net_mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            msg_id = esp_mqtt_client_subscribe(client, "ratt/control/broadcast/acl/update", 0);
            msg_id = esp_mqtt_client_subscribe(client, "ratt/control/broadcast/firmware/update", 0);
            ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            display_mqtt_status(MQTT_STATUS_CONNECTED);
            s_mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from MQTT broker");

            display_mqtt_status(MQTT_STATUS_DISCONNECTED);

            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);

            display_mqtt_status(MQTT_STATUS_DATA_SENT);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

            // note using strncmp on non-null-terminated char arrays here...
            if (strncmp(event->topic, "ratt/control/broadcast/acl/update", event->topic_len) == 0) {
              net_cmd_queue(NET_CMD_DOWNLOAD_ACL);
            } else if (strncmp(event->topic, "ratt/control/broadcast/firmware/update", event->topic_len) == 0) {
              main_task_event(MAIN_EVT_OTA_UPDATE);
            }

            display_mqtt_status(MQTT_STATUS_DATA_RECEIVED);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            display_mqtt_status(MQTT_STATUS_ERROR);
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}


int net_mqtt_init(void)
{
  ESP_LOGI(TAG, "Initializing MQTT...");

  display_mqtt_status(MQTT_STATUS_INIT);

  esp_log_level_set("MQTT_CLIENT", ESP_LOG_WARN);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_WARN);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_WARN);
  esp_log_level_set("TRANSPORT", ESP_LOG_WARN);
  esp_log_level_set("OUTBOX", ESP_LOG_WARN);

  esp_mqtt_client_config_t mqtt_cfg = {
      .event_handle = net_mqtt_event_handler,
      .client_cert_pem = g_client_cert,
      .client_key_pem = g_client_key,
      .cert_pem = g_ca_cert,
      .skip_cert_common_name_check = true,
  };

  char *conf_mqtt_broker;
  config_get_string("mqtt_broker", &conf_mqtt_broker, "mqtts://my-mqtt-server.org:1883");

  ESP_LOGI(TAG, "MQTT broker is %s", conf_mqtt_broker);

  mqtt_cfg.uri = conf_mqtt_broker;

  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

  esp_log_level_set(TAG, ESP_LOG_INFO);

  free(conf_mqtt_broker);

  return 0;
}

int net_mqtt_start(void)
{
  ESP_LOGI(TAG, "Starting MQTT client.");
  return esp_mqtt_client_start(s_mqtt_client);
}

int net_mqtt_stop(void)
{
  ESP_LOGI(TAG, "Stopping MQTT client.");
  s_mqtt_connected = false;
  display_mqtt_status(MQTT_STATUS_DISCONNECTED);
  return esp_mqtt_client_stop(s_mqtt_client);
}
