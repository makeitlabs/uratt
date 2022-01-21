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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"

static const char *TAG = "spiflash";

// Handles of the wear levelling library instance
static wl_handle_t s_config_wl_handle = WL_INVALID_HANDLE;

// Mount paths for the partitions
const char *config_path = "/config";

// Flash partition names, from partitions.csv
const char *config_partition = "config";

static esp_err_t spiflash_mount(const char* partition, const char* path, wl_handle_t* handle)
{
  // To mount device we need name of device partition, define base_path
  // and allow format partition in case if it is new one and was not formatted before
  const esp_vfs_fat_mount_config_t mount_config = {
          .max_files = 4,
          .format_if_mount_failed = true,
          .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
  };
  esp_err_t err = esp_vfs_fat_spiflash_mount(path, partition, &mount_config, handle);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to mount spiflash partition %s (%s)", partition, esp_err_to_name(err));
      return err;
  }
  ESP_LOGI(TAG, "Mounted spiflash partition '%s' to '%s'", partition, path);

  return ESP_OK;
}

esp_err_t spiflash_init(void)
{
    ESP_LOGI(TAG, "Mounting SPI Flash FAT filesystems...");

    return spiflash_mount(config_partition, config_path, &s_config_wl_handle);
}


static esp_err_t spiflash_unmount(const char* partition, const char* path, wl_handle_t* handle)
{
  ESP_LOGI(TAG, "Unmounting spiflash partition %s...", partition);
  esp_err_t err = esp_vfs_fat_spiflash_unmount(path, *handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount spiflash partition %s (%s)", partition, esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "Unmounted spiflash partition '%s'.", partition);
  return ESP_OK;
}

esp_err_t spiflash_deinit(void)
{
  return spiflash_unmount(config_partition, config_path, &s_config_wl_handle);
}
