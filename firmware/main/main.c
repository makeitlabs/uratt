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
#include "system.h"

#include "console.h"
#include "sdcard.h"
#include "spiflash.h"
#include "display_task.h"
#include "net_task.h"
#include "rfid_task.h"
#include "door_task.h"
#include "beep_task.h"
#include "system_task.h"
#include "main_task.h"

static const char *TAG = "main";


static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// experimental windowed logging/command line
/*
int windowed_vprintf(const char* format, va_list vlist)
{
  printf("\033[s\033[1;25r\033D\033[25H");
  int r = vprintf(format, vlist);
  printf("\033[26;36r\033[u");
  fflush(stdout);
  return r;
}
*/

void app_main(void)
{
    /*
    printf("\033[2J");
    esp_log_set_vprintf(windowed_vprintf);
    */

    ESP_LOGI(TAG, "initializing");

    nvs_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    system_init();
#ifdef DISPLAY_ENABLED
    display_init();
#endif
    beep_init();
    door_init();

    sdcard_init();
    spiflash_init();
    rfid_init();
    main_task_init();

    ESP_LOGI(TAG, "creating tasks");

    xTaskCreate(&system_task, "system_task", 2048, NULL, 8, NULL);
    xTaskCreate(&beep_task, "beep_task", 2560, NULL, 8, NULL);
    xTaskCreate(&door_task, "door_task", 2048, NULL, 8, NULL);
    xTaskCreate(&rfid_task, "rfid_task", 3072, NULL, 8, NULL);
#ifdef DISPLAY_ENABLED
    xTaskCreate(&display_task, "display_task", 4096, NULL, 8, NULL);
#endif
    xTaskCreate(&net_task, "net_task", 4096, NULL, 7, NULL);
    xTaskCreate(&main_task, "main_task", 2048, NULL, 7, NULL);

    ESP_LOGW(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

#ifdef CONSOLE_ENABLED
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    console_init();

    while (true) {
      console_poll();
    }

    ESP_LOGI(TAG, "Console terminated.");
    console_done();
#endif
}
