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

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"
#include "system.h"
#include "system_task.h"
#include "spiflash.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "main_task.h"
#include "display_task.h"

static const char *TAG = "system_task";

#define SYSTEM_QUEUE_DEPTH 8

typedef struct system_evt {
  int cmd;
} system_evt_t;

static QueueHandle_t m_q;



void power_mgmt_init(void)
{
  gpio_reset_pin(GPIO_PIN_SHUTDOWN);
  gpio_set_direction(GPIO_PIN_SHUTDOWN, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PIN_SHUTDOWN, 1);

  gpio_reset_pin(GPIO_PIN_PWR_ENABLE);
  gpio_set_direction(GPIO_PIN_PWR_ENABLE, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PIN_PWR_ENABLE, 1);

  gpio_reset_pin(GPIO_PIN_N_PWR_LOSS);
  gpio_set_direction(GPIO_PIN_N_PWR_LOSS, GPIO_MODE_INPUT);

  gpio_reset_pin(GPIO_PIN_LOW_BAT);
  gpio_set_direction(GPIO_PIN_LOW_BAT, GPIO_MODE_INPUT);
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void system_init(void)
{
  nvs_init();
  spiflash_init();

  m_q = xQueueCreate(SYSTEM_QUEUE_DEPTH, sizeof(system_evt_t));
  if (m_q == NULL) {
      ESP_LOGE(TAG, "FATAL: Cannot create system task queue!");
  }

  power_mgmt_init();
}


void sdelay(int ms)
{
    TickType_t delay = ms / portTICK_PERIOD_MS;
    vTaskDelay(delay);
}

void system_task(void *pvParameters)
{
  ESP_LOGI(TAG, "System management task started");

  uint last_pwr_loss = 1;
  uint last_low_batt = 1;

  time_t time_pwr_loss = 0;
  time_t time_low_batt = 0;

  time_t last_time_pwr_loss = 0;
  time_t last_time_low_batt = 0;

  time_t last_lost = 0;

  time_t now = 0;


  while(1) {
    time(&now);
    if (last_time_pwr_loss == 0) {
      last_time_pwr_loss = now;
    }
    if (last_time_low_batt == 0) {
      last_time_low_batt = now;
    }

    system_evt_t evt;

    if (xQueueReceive(m_q, &evt, (20 / portTICK_PERIOD_MS)) == pdPASS) {
      ESP_LOGI(TAG, "system task cmd=%d\n", evt.cmd);
    }

    uint pwr_loss = gpio_get_level(GPIO_PIN_N_PWR_LOSS);

    if (pwr_loss != last_pwr_loss) {
      if (pwr_loss == 0) {
        main_task_event(MAIN_EVT_POWER_LOSS);
        ESP_LOGW(TAG, "power lost!");
        time_pwr_loss = now;
        last_time_pwr_loss = now;
      } else {
        main_task_event(MAIN_EVT_POWER_RESTORED);
        ESP_LOGI(TAG, "power restored!");
      }

      last_pwr_loss = pwr_loss;
      last_lost = now;
    }

    if (pwr_loss == 0) {
        time_t lost = (now - last_time_pwr_loss);
        if (lost != last_lost) {
          if (lost < 30) {
            ESP_LOGW(TAG, "power lost for %ld seconds", now - time_pwr_loss);
            display_sleep_countdown(30 - lost);
          } else if (lost >= 30) {
            main_task_event(MAIN_EVT_SLEEPING);
            taskYIELD();

            ESP_LOGI(TAG, "going to sleep");

            esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
            gpio_wakeup_enable(13, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable(35, GPIO_INTR_HIGH_LEVEL);
            esp_sleep_enable_gpio_wakeup();

            gpio_set_level(GPIO_PIN_PWR_ENABLE, 0);

            //lcd_reset();
            //lcd_set_backlight(OFF);

            esp_light_sleep_start();

            ESP_LOGI(TAG, "waking up");

            gpio_set_level(GPIO_PIN_PWR_ENABLE, 1);

            //lcd_reset();
            //lcd_reinit();

            //display_redraw_bg();

            //lcd_set_backlight(ON);

            main_task_event(MAIN_EVT_WAKING);
            taskYIELD();

            last_time_pwr_loss = now;
          }
        }
        last_lost = lost;
    }


    uint low_batt = gpio_get_level(GPIO_PIN_LOW_BAT);
    if (low_batt != last_low_batt) {
      if (low_batt == 0) {
        ESP_LOGW(TAG, "low battery detected!");
        time_low_batt = now;
        last_time_low_batt = now;
      } else {
        ESP_LOGI(TAG, "battery voltage OK!");
      }
  }
  if (low_batt == 0) {
    if ((now - last_time_low_batt) >= 10) {
      ESP_LOGE(TAG, "low battery for %ld seconds", now - time_low_batt);
      last_time_low_batt = now;
    }

    if ((now - time_low_batt) >= 60) {
      ESP_LOGW(TAG, "Shutting down in 5 seconds...");
      sdelay(5);
      gpio_set_level(GPIO_PIN_SHUTDOWN, 0);
    }
  }
  last_low_batt = low_batt;
}
}
