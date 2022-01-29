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
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mbedtls/md.h"

#include "config.h"
#include "display_task.h"
#include "acl.h"

static const char *TAG = "acl";

const size_t sha224_len = (56 + 1);
SemaphoreHandle_t g_acl_mutex;

esp_err_t acl_init(void)
{
    g_acl_mutex = xSemaphoreCreateMutex();
    if (!g_acl_mutex) {
        ESP_LOGE(TAG, "Could not create mutexes.");
        return ESP_FAIL;
    }

    if (acl_validate() == ESP_OK) {
      display_acl_status(ACL_STATUS_CACHED, 100);
    } else {
      display_acl_status(ACL_STATUS_ERROR, 0);
    }

    return ESP_OK;
}

esp_err_t acl_get_data_filename(char **s)
{
  return config_get_string("acl_file", s, "/config/acl.csv");
}

esp_err_t acl_get_hash_filename(char **s)
{
  return config_get_string("acl_hash_file", s, "/config/acl.sha");
}

// Load the stored ACL hash from a file
// MUST hold the g_acl_mutex before calling!
esp_err_t acl_get_stored_hash__acl_mutex(const char* filename, char *hash)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
      ESP_LOGW(TAG, "can't open ACL hash file %s for read, may not exist yet.", filename);
      return ESP_FAIL;
  }

  int r = read(fd, hash, sha224_len);
  if (r < 0) {
    ESP_LOGE(TAG, "error reading ACL hash file %s", filename);
    close(fd);
    return ESP_FAIL;
  }

  close(fd);

  // null fence for safety
  hash[sha224_len - 1] = '\0';
  ESP_LOGI(TAG, "Stored ACL hash is %s", hash);
  return ESP_OK;
}

// Compute the hash of the stored ACL data file
// MUST hold the g_acl_mutex before calling!
esp_err_t acl_compute_stored_hash__acl_mutex(const char* filename, char *hash)
{
  mbedtls_md_context_t md_ctx;

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
      ESP_LOGW(TAG, "can't open ACL hash file %s for read, may not exist yet.", filename);
      return ESP_FAIL;
  }

  mbedtls_md_init(&md_ctx);
  if (mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA224), 0) != ESP_OK ||
      mbedtls_md_starts(&md_ctx) != ESP_OK) {
      ESP_LOGE(TAG, "error setting up mbedtls");
      return ESP_FAIL;
  }

  unsigned char *buf = malloc(256);
  int r = -1;
  while (1) {
    r = read(fd, buf, 256);
    if (r == 0) break;
    else if (r < 0) {
      ESP_LOGE(TAG, "error reading ACL hash file %s", filename);
      break;
    } else {
      if (mbedtls_md_update(&md_ctx, buf, r) != ESP_OK) {
        ESP_LOGE(TAG, "error computing sha224 on %d bytes of data", r);
        r = -1;
        break;
      }
    }
  }
  free(buf);
  close(fd);

  if (r == 0) {
    uint8_t hbuf[sha224_len / 2];
    mbedtls_md_finish(&md_ctx, hbuf);
    mbedtls_md_free(&md_ctx);

    for (uint8_t idx=0; idx<(sha224_len / 2); idx++) {
      sprintf(hash + (idx * 2), "%2.2x", hbuf[idx]);
    }
    hash[sha224_len - 1] = '\0';
    ESP_LOGI(TAG, "Calculated hash of ACL file is %s", hash);

    return ESP_OK;
  }

  return ESP_FAIL;
}

esp_err_t acl_validate(void)
{
  esp_err_t r = ESP_FAIL;

  char *stored_hash;
  char *computed_hash;
  char *conf_acl_filename;
  char *conf_acl_hash_filename;

  stored_hash = malloc(sha224_len);
  computed_hash = malloc(sha224_len);

  acl_get_data_filename(&conf_acl_filename);
  acl_get_hash_filename(&conf_acl_hash_filename);

  xSemaphoreTake(g_acl_mutex, portMAX_DELAY);

  // get the stored hash
  if (acl_get_stored_hash__acl_mutex(conf_acl_hash_filename, stored_hash) != ESP_OK) {
    ESP_LOGW(TAG, "Couldn't load stored hash; considering stored ACL invalid");
    goto failed;
  }

  if (acl_compute_stored_hash__acl_mutex(conf_acl_filename, computed_hash) != ESP_OK) {
    ESP_LOGW(TAG, "Couldn't computed hash of stored ACL; considering stored ACL invalid.");
    goto failed;
  }

  if (strcmp(stored_hash, computed_hash) == 0) {
    ESP_LOGI(TAG, "Stored ACL hashes validated.");
    r = ESP_OK;
    goto done;
  }

failed:
  ESP_LOGW(TAG, "Stored ACL hashes not valid.  Removing stored files.");

  if (unlink(conf_acl_filename) != 0) {
    ESP_LOGE(TAG, "Could not delete ACL file %s", conf_acl_filename);
  }
  if (unlink(conf_acl_hash_filename) != 0) {
    ESP_LOGE(TAG, "Could not delete ACL hash file %s", conf_acl_hash_filename);
  }


done:
  xSemaphoreGive(g_acl_mutex);
  free(conf_acl_filename);
  free(conf_acl_hash_filename);
  free(stored_hash);
  free(computed_hash);
  return r;
}
