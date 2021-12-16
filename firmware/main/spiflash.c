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

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";

// Flash partition name, from partitions.csv
const char *flash_partition = "storage";

esp_err_t spiflash_init(void)
{
    ESP_LOGI(TAG, "Mounting SPI Flash FAT filesystem...");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, flash_partition, &mount_config, &s_wl_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPI Flash FATFS (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Mounted SPI Flash partition '%s' to '%s'", flash_partition, base_path);
    return ESP_OK;
}

esp_err_t spiflash_deinit(void)
{
  // Unmount FATFS
  ESP_LOGI(TAG, "Unmounting SPI Flash FAT filesystem...");
  esp_err_t err = esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount SPI Flash FATFS (%s)", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "Unmounted SPI Flash from '%s'.", base_path);
  return ESP_OK;
}
