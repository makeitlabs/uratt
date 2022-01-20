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
#include "esp_task_wdt.h"
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
#include "display_lvgl.h"

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

void system_pre_sleep()
{
  display_lvgl_disp_off(true);
  vTaskDelay(500/portTICK_PERIOD_MS);
}

void system_sleep()
{
  ESP_LOGI(TAG, "going to sleep");
  fflush(stdout);
  vTaskDelay(500/portTICK_PERIOD_MS);

  gpio_set_level(GPIO_PIN_PWR_ENABLE, 0);

  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
  gpio_wakeup_enable(GPIO_PIN_FP_BUTTON, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(GPIO_PIN_N_PWR_LOSS, GPIO_INTR_HIGH_LEVEL);

  int alrm = gpio_get_level(GPIO_PIN_ALARM_SCL);
  gpio_wakeup_enable(GPIO_PIN_ALARM_SCL, alrm ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);

  esp_sleep_enable_gpio_wakeup();

  esp_light_sleep_start();
}

void system_wake()
{
  ESP_LOGI(TAG, "waking up, reason=%d", esp_sleep_get_wakeup_cause());

  gpio_set_level(GPIO_PIN_PWR_ENABLE, 1);

  display_lvgl_disp_off(false);

  vTaskDelay(500/portTICK_PERIOD_MS);
}

void system_shutdown()
{
  gpio_set_level(GPIO_PIN_SHUTDOWN, 0);

}

void system_task(void *pvParameters)
{
  ESP_LOGI(TAG, "System management task started");

  uint last_pwr_loss = 1;
  uint last_low_batt = 1;

  time_t last_pwr_loss_change_time = 0;
  time_t last_low_batt_change_time = 0;

  esp_task_wdt_add(NULL);

  while(1) {
    system_evt_t evt;

    esp_task_wdt_reset();

    if (xQueueReceive(m_q, &evt, (20 / portTICK_PERIOD_MS)) == pdPASS) {
      ESP_LOGI(TAG, "system task cmd=%d\n", evt.cmd);
    }

    uint pwr_loss = gpio_get_level(GPIO_PIN_N_PWR_LOSS);
    if (pwr_loss != last_pwr_loss) {
      time_t now;
      time(&now);

      if (now - last_pwr_loss_change_time >= 2) {
        // only allow power state to change every 2 seconds, essentially a debounce
        if (pwr_loss == 0) {
          main_task_event(MAIN_EVT_POWER_LOSS);
          ESP_LOGW(TAG, "power lost!");
        } else {
          main_task_event(MAIN_EVT_POWER_RESTORED);
          ESP_LOGI(TAG, "power restored!");
        }
        last_pwr_loss = pwr_loss;
      }
    } else {
      time(&last_pwr_loss_change_time);
    }

    uint low_batt = gpio_get_level(GPIO_PIN_LOW_BAT);
    if (low_batt != last_low_batt) {
      time_t now;
      time(&now);
      if (now - last_low_batt_change_time >= 2) {
        if (low_batt == 0) {
          main_task_event(MAIN_EVT_BATTERY_LOW);
          ESP_LOGW(TAG, "low battery detected!");
        } else {
          main_task_event(MAIN_EVT_BATTERY_OK);
          ESP_LOGI(TAG, "battery voltage OK!");
        }
        last_low_batt = low_batt;
      }
    } else {
      time(&last_low_batt_change_time);
    }

  }
}
