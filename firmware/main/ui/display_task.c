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
#include "esp_log.h"
#include "system.h"
#include "display_lvgl.h"
#include "lvgl.h"
#include "beep_task.h"
#include "ui_splash.h"
#include "ui_idle.h"
#include "ui_access.h"
#include "ui_sleep.h"

#ifdef DISPLAY_ENABLED
static const char *TAG = "display_task";

#define DISPLAY_QUEUE_DEPTH 4
#define DISPLAY_EVT_BUF_SIZE 32


typedef enum {
    DISP_CMD_WIFI_STATUS = 0x00,
    DISP_CMD_WIFI_RSSI,
    DISP_CMD_ACL_STATUS,
    DISP_CMD_MQTT_STATUS,
    DISP_CMD_CHARGE_STATUS,
    DISP_CMD_SLEEP_COUNTDOWN,
    DISP_CMD_ALLOWED_MSG,
    DISP_CMD_SHOW_IDLE,
    DISP_CMD_SHOW_ACCESS,
    DISP_CMD_SHOW_SLEEP
} display_cmd_t;

typedef struct display_evt {
    display_cmd_t cmd;
    char buf[DISPLAY_EVT_BUF_SIZE];
    union {
        acl_status_t acl_status;
        mqtt_status_t mqtt_status;
        wifi_status_t wifi_status;
        uint8_t progress;
        int16_t rssi;
        uint8_t allowed;
        uint16_t delay;
        uint8_t countdown;
    } params;
    union {
        bool destroy;
    } extparams;
} display_evt_t;

static lv_obj_t *s_scr = NULL;

static QueueHandle_t m_q;
#endif

BaseType_t display_wifi_status(wifi_status_t status)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_WIFI_STATUS;
    evt.params.wifi_status = status;

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_wifi_rssi(int16_t rssi)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_WIFI_RSSI;
    evt.params.rssi = rssi;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_acl_status(acl_status_t status)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_ACL_STATUS;
    evt.params.acl_status = status;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif


BaseType_t display_mqtt_status(mqtt_status_t status)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_MQTT_STATUS;
    evt.params.mqtt_status = status;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_sleep_countdown(uint8_t countdown)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_SLEEP_COUNTDOWN;
    evt.params.countdown = countdown;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_allowed_msg(char *msg, uint8_t allowed)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_ALLOWED_MSG;
    evt.params.allowed = allowed;
    strncpy(evt.buf, msg, DISPLAY_EVT_BUF_SIZE);

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_show_idle(uint16_t delay, bool destroy)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.params.delay = delay;
    evt.extparams.destroy = destroy;
    evt.cmd = DISP_CMD_SHOW_IDLE;

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_show_access()
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_SHOW_ACCESS;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif

BaseType_t display_show_sleep()
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_SHOW_SLEEP;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif


void display_init()
#ifdef DISPLAY_ENABLED
{
    gpio_set_direction(GPIO_PIN_FP_BUTTON, GPIO_MODE_INPUT);
    gpio_pullup_en(GPIO_PIN_FP_BUTTON);

    m_q = xQueueCreate(DISPLAY_QUEUE_DEPTH, sizeof(display_evt_t));
    if (m_q == NULL) {
        ESP_LOGE(TAG, "FATAL: Cannot create display queue!");
    }

    s_scr = display_lvgl_init_scr();
}
#else
{ }
#endif


void display_task(void *pvParameters)
#ifdef DISPLAY_ENABLED
{
    portTickType init_tick = xTaskGetTickCount();
    portTickType last_heartbeat_tick = init_tick;
    int button=0, last_button=0;


    lv_obj_t *scr_splash = ui_splash_create();
    lv_scr_load(scr_splash);

    lv_obj_t *scr_idle = ui_idle_create();

    while(1) {
        display_evt_t evt;

        // braindead button test
        button = gpio_get_level(GPIO_PIN_FP_BUTTON);

        if (button != last_button) {
            ESP_LOGI(TAG, "Button now=%d", button);
            if (button == 0)
              beep_queue(2000, 75, 1, 1);
        }

        last_button = button;
        // end button test

        display_lvgl_periodic();

        if (xQueueReceive(m_q, &evt, (10 / portTICK_PERIOD_MS)) == pdPASS) {
            switch(evt.cmd) {
            case DISP_CMD_WIFI_STATUS:
                ui_idle_set_wifi_status(evt.params.wifi_status);
                break;
            case DISP_CMD_WIFI_RSSI:
                ui_idle_set_rssi(evt.params.rssi);
                break;
            case DISP_CMD_ACL_STATUS:
                ui_idle_set_acl_status(evt.params.acl_status);
                break;
            case DISP_CMD_MQTT_STATUS:
                ui_idle_set_mqtt_status(evt.params.mqtt_status);
                break;
            case DISP_CMD_CHARGE_STATUS:
                break;
            case DISP_CMD_SLEEP_COUNTDOWN:
                ui_sleep_set_countdown(evt.params.countdown);
                break;
            case DISP_CMD_ALLOWED_MSG:
                ui_access_set_user(evt.buf, evt.params.allowed);
                break;
            case DISP_CMD_SHOW_IDLE:
                lv_scr_load_anim(scr_idle, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 750, evt.params.delay, evt.extparams.destroy);
                break;
            case DISP_CMD_SHOW_ACCESS:
                {
                  lv_obj_t *scr_access = ui_access_create();
                  lv_scr_load_anim(scr_access, LV_SCR_LOAD_ANIM_MOVE_TOP, 750, 0, false);
                }
                break;
            case DISP_CMD_SHOW_SLEEP:
                {
                  lv_obj_t *scr_sleep = ui_sleep_create();
                  lv_scr_load_anim(scr_sleep, LV_SCR_LOAD_ANIM_FADE_ON, 1000, 0, false);
                }
                break;
            }
        }

        portTickType now = xTaskGetTickCount();


        if (now - last_heartbeat_tick >= (1000/portTICK_PERIOD_MS)) {
            // heartbeat

            char strftime_buf[20];
            time_t tnow;
            struct tm timeinfo;

            time(&tnow);
            setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
            tzset();
            localtime_r(&tnow, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%l:%M%P", &timeinfo);

            ui_idle_set_time(strftime_buf);

            // strftime_buf is time
            last_heartbeat_tick = now;
        }
    }
}
#else
{  }
#endif
