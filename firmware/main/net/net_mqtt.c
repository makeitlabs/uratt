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

#include "mqtt_client.h"

#include "rfid_task.h"
#include "net_task.h"
#include "net_mqtt.h"
#include "net_certs.h"

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
  if (s_mqtt_connected) {
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
      if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
        ESP_LOGD(TAG, "published wifi status");
      } else {
        ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
      }

      free(topic);
      free(payload);
    }
  }
}


void net_mqtt_send_acl_updated(char* status)
{
  if (s_mqtt_connected) {
    // ratt/status/node/b827eb2f8dca/acl/update {"status":"downloaded"}

    char *topic, *payload;
    topic = malloc(128);
    payload = malloc(128);

    net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "acl/update", topic, 128);

    snprintf(payload, 128, "{\"status\":\"%s\"}", status);
    if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
      ESP_LOGD(TAG, "published acl update status");
    } else {
      ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
    }

    free(topic);
    free(payload);
  }
}


void net_mqtt_send_access(char *member, int allowed)
{
  if (s_mqtt_connected) {
    char *topic, *payload;
    topic = malloc(128);
    payload = malloc(128);

    net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "personality/access", topic, 128);

    snprintf(payload, 128, "{\"member\": \"%s\", \"allowed\": %s}", member, allowed ? "true" : "false");
    if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
      ESP_LOGD(TAG, "published personality access");
    } else {
      ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
    }

    free(topic);
    free(payload);
  }
}

void net_mqtt_send_access_error(char *err_text, char *err_ext)
{
  if (s_mqtt_connected) {
    char *topic, *payload;
    topic = malloc(128);
    payload = malloc(128);

    net_mqtt_topic_targeted(MQTT_TOPIC_TYPE_STATUS, "personality/access", topic, 128);

    snprintf(payload, 128, "{\"error\": true, \"errorText\": \"%s\", \"errorExt\": \"%s\"}", err_text, err_ext);
    if (esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 2, 0) != -1) {
      ESP_LOGD(TAG, "published personality access error");
    } else {
      ESP_LOGE(TAG, "error publishing to topic '%s'", topic);
    }

    free(topic);
    free(payload);
  }
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
            ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            s_mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from MQTT broker");
            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

            net_cmd_queue(NET_CMD_DOWNLOAD_ACL);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
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
  return esp_mqtt_client_start(s_mqtt_client);
}

int net_mqtt_stop(void)
{
  return esp_mqtt_client_stop(s_mqtt_client);
}
