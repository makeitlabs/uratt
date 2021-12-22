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
#include "https.h"
#include "sdcard.h"

#include "net_certs.h"
#include "config.h"
#include "rfid_task.h"
#include "net_task.h"
#include "net_https.h"
#include "display_task.h"

static const char *TAG = "net_https";

static const size_t url_len = 256;
static const size_t sha224_len = (56 + 1);

int net_https_init(void)
{
  ESP_LOGI(TAG, "net_https init");

  display_acl_status(ACL_STATUS_INIT, 0);

  http_init();

  return 0;
}

void acl_progress(int received, int total)
{
  static int last_percent = -1;
  int percent = received * 100 / total;

  if (percent != last_percent) {
    display_acl_status(ACL_STATUS_DOWNLOADING, percent);
  }

  last_percent = percent;
}

esp_err_t net_https_get_acl_filename(char **s)
{
  return config_get_string("acl_file", s, "/config/acl.csv");
}

esp_err_t net_https_download_acl()
{
  int r = ESP_FAIL;
  char *url = NULL;
  char *hash_buf = NULL;
  char *hash_expected = NULL;
  char *conf_acl_url_fmt = NULL;
  char *conf_acl_resource = NULL;
  char *conf_acl_filename = NULL;
  char *conf_acl_temp_filename = NULL;
  char *conf_acl_hash_filename = NULL;
  char *conf_api_user = NULL;
  char *conf_api_password = NULL;

  url = malloc(url_len);
  hash_buf = malloc(sha224_len);
  hash_expected = malloc(sha224_len);

  // construct the URL
  config_get_string("acl_url_fmt", &conf_acl_url_fmt, "https://my-server.org:443/auth/api/v0/resources/%s/acl");
  config_get_string("acl_resource", &conf_acl_resource, "frontdoor");
  snprintf(url, url_len, conf_acl_url_fmt, conf_acl_resource);

  net_https_get_acl_filename(&conf_acl_filename);
  config_get_string("acl_temp_file", &conf_acl_temp_filename, "/config/acltemp.csv");
  config_get_string("acl_hash_file", &conf_acl_hash_filename, "/config/aclhash.txt");

  config_get_string("api_user", &conf_api_user, "username");
  config_get_string("api_password", &conf_api_password, "password");

  // Load the stored ACL hash from a file
  xSemaphoreTake(g_acl_mutex, portMAX_DELAY);
  int fd = open(conf_acl_hash_filename, O_RDONLY);
  if (fd < 0) {
      ESP_LOGI(TAG, "can't open ACL hash file for read, may not exist yet.");
      strncpy(hash_expected, "none", sha224_len);
  } else {
    int r = read(fd, hash_expected, sha224_len);
    if (r < 0) {
      ESP_LOGE(TAG, "error reading ACL hash file");
      strncpy(hash_expected, "none", sha224_len);
    }
    close(fd);

    // null fence for safety
    hash_expected[sha224_len - 1] = '\0';
    ESP_LOGI(TAG, "Stored ACL hash is %s", hash_expected);
  }
  xSemaphoreGive(g_acl_mutex);


  // Build the HTTP(s) request
  http_get_req_t req = {
    .url = url,
    .auth_user = conf_api_user,
    .auth_password = conf_api_password,
    .filename = conf_acl_temp_filename,
    .client_cert_pem = g_client_cert,
    .client_key_pem = g_client_key,
    .ca_cert_pem = g_ca_cert,
    .ssl_insecure = true,

    .hash_expected = hash_expected,
    .hash_expected_cancel = true,

    .progress_cb = acl_progress,

    .resp_hash_buf = hash_buf
  };

  ESP_LOGI(TAG, "download ACL from URL: %s", url);
  display_acl_status(ACL_STATUS_DOWNLOADING, 0);

  r = http_get(&req);

  ESP_LOGD(TAG, "http_get returned %d, %d bytes, hash matched=%s", req.resp_status, req.resp_content_length, req.resp_hash_expected_match ? "TRUE" : "FALSE");
  if (r != ESP_OK) {
    goto failed;
  }

  if (req.resp_status == 200) {
    if (req.resp_hash_expected_match) {
      ESP_LOGI(TAG, "Remote and stored ACL have same hash.  No need to update.");
      display_acl_status(ACL_STATUS_DOWNLOADED_SAME_HASH, 100);
      net_cmd_queue(NET_CMD_SEND_ACL_UPDATED);
    } else {
      xSemaphoreTake(g_acl_mutex, portMAX_DELAY);

      // delete existing ACL file if it exists
      struct stat st;
      if (stat(conf_acl_filename, &st) == 0) {
        if (unlink(conf_acl_filename) != 0) {
          ESP_LOGE(TAG, "Could not delete old ACL file %s", conf_acl_filename);
          xSemaphoreGive(g_acl_mutex);
          goto failed;
        }
      }

      ESP_LOGD(TAG, "Deleted old ACL file %s", conf_acl_filename);

      // move downloaded ACL in place
      if (rename(conf_acl_temp_filename, conf_acl_filename) != 0) {
        ESP_LOGE(TAG, "Could not rename downloaded ACL file %s -> %s", conf_acl_temp_filename, conf_acl_filename);
        xSemaphoreGive(g_acl_mutex);
        goto failed;
      }

      ESP_LOGD(TAG, "Moved temporary ACL file %s -> %s.", conf_acl_temp_filename, conf_acl_filename);

      // save the hash of the ACL to a separate file
      int fd = open(conf_acl_hash_filename, O_WRONLY|O_CREAT|O_TRUNC);
      if (fd < 0) {
          ESP_LOGE(TAG, "Can't open ACL hash file %s for write", conf_acl_hash_filename);
          xSemaphoreGive(g_acl_mutex);
          goto failed;
      }
      int r = write(fd, req.resp_hash_buf, strlen(req.resp_hash_buf)+1);
      if (r < 0) {
        ESP_LOGE(TAG, "Error writing to ACL hash file %s", conf_acl_hash_filename);
        close(fd);
        xSemaphoreGive(g_acl_mutex);
        goto failed;
      }
      close(fd);
      xSemaphoreGive(g_acl_mutex);

      display_acl_status(ACL_STATUS_DOWNLOADED_UPDATED, 100);
      net_cmd_queue(NET_CMD_SEND_ACL_UPDATED);
    }

  } else {
    ESP_LOGE(TAG, "http_get status %d, download ACL failed", req.resp_status);
    goto failed;
  }

  ESP_LOGI(TAG, "download ACL success!");
  r = ESP_OK;
  goto done;


failed:
  ESP_LOGE(TAG, "download ACL failure.");
  display_acl_status(ACL_STATUS_ERROR, 0);
  net_cmd_queue(NET_CMD_SEND_ACL_FAILED);

done:
  free(url);
  free(hash_buf);
  free(hash_expected);
  free(conf_acl_filename);
  free(conf_acl_temp_filename);
  free(conf_acl_hash_filename);
  free(conf_acl_url_fmt);
  free(conf_acl_resource);
  free(conf_api_user);
  free(conf_api_password);

  return r;
}


int net_https_get_file(const char *web_url, const char *filename)
{
  int result = 0;
  int r = ESP_FAIL;

  ESP_LOGI(TAG, "get file from URL: %s", web_url);

  http_get_req_t req = {
    .url = web_url,
    .auth_user = NULL,
    .auth_password = NULL,
    .filename = filename,
    .client_cert_pem = NULL,
    .client_key_pem = NULL,
    .ca_cert_pem = NULL,
    .ssl_insecure = false,

    .progress_cb = NULL,

    .resp_hash_buf = NULL,
  };

  r = http_get(&req);

  ESP_LOGI(TAG, "http_get returned %d", req.resp_status);
  if (r != ESP_OK) {
    goto failed;
  }

  if (req.resp_status == 200) {
    ESP_LOGI(TAG, "download OK!");
  } else {
    ESP_LOGE(TAG, "http result code %d", result);
    goto failed;
  }

  ESP_LOGI(TAG, "download file %s success!", filename);
  return 0;

  failed:
  return r;
}
