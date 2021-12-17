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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"

static const char *TAG = "config";

// Mount path for the partition
const char *nvs_namespace = "config";

esp_err_t config_get_string(const char* key, char **str, char* def_val)
{
  nvs_handle_t hdl;

  *str = NULL;

  esp_err_t r = nvs_open(nvs_namespace, NVS_READWRITE, &hdl);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "NVS open error: %s", esp_err_to_name(r));
    goto exit;
  }

  size_t req_len;
  // when called with the buffer set to null, it will return the required length to store the value
  r = nvs_get_str(hdl, key, NULL, &req_len);
  if (r == ESP_ERR_NVS_NOT_FOUND) {
    if (def_val == NULL) {
      ESP_LOGE(TAG, "nvs config key doesn't exist and no default value specified");
      goto exit;
    }
    r = nvs_set_str(hdl, key, def_val);
    if (r != ESP_OK) {
      ESP_LOGE(TAG, "nvs_set_str error: %s", esp_err_to_name(r));
      goto exit;
    }

    r = nvs_commit(hdl);
    if (r != ESP_OK) {
      ESP_LOGE(TAG, "nvs_commit error: %s", esp_err_to_name(r));
      goto exit;
    }

    req_len = strlen(def_val) + 1;
    ESP_LOGI(TAG, "create new config option key '%s' default value '%s'", key, def_val);

  } else if (r != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_str error: %s", esp_err_to_name(r));
    goto exit;
  }

  *str = malloc(req_len);
  if (*str == NULL) {
    ESP_LOGE(TAG, "can't malloc %d bytes in config_get_string", req_len);
    goto exit;
  }

  r = nvs_get_str(hdl, key, *str, &req_len);
  if (r != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_str error: %s", esp_err_to_name(r));
    goto exit;
  }

exit:
  nvs_close(hdl);
  return r;
}
