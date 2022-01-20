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
#include "esp_task_wdt.h"
#include "system.h"
#include "display_lvgl.h"
#include "lvgl.h"
#include "main_task.h"
#include "beep_task.h"
#include "ui_blank.h"
#include "ui_splash.h"
#include "ui_idle.h"
#include "ui_access.h"
#include "ui_info.h"

#ifdef DISPLAY_ENABLED
static const char *TAG = "display_task";

#define DISPLAY_QUEUE_DEPTH 4
#define DISPLAY_EVT_BUF_SIZE 32

typedef enum {
    DISP_CMD_WIFI_STATUS = 0x00,
    DISP_CMD_WIFI_RSSI,
    DISP_CMD_NET_STATUS,
    DISP_CMD_ACL_STATUS,
    DISP_CMD_MQTT_STATUS,
    DISP_CMD_POWER_STATUS,
    DISP_CMD_ALLOWED_MSG,
    DISP_CMD_DOOR_STATE,
    DISP_CMD_SHOW_SCREEN,
} display_cmd_t;

typedef struct display_evt {
    display_cmd_t cmd;
    char buf[DISPLAY_EVT_BUF_SIZE];
    union {
        screen_t screen;
        power_status_t power_status;
        acl_status_t acl_status;
        mqtt_status_t mqtt_status;
        wifi_status_t wifi_status;
        net_status_t net_status;
        uint8_t progress;
        int16_t rssi;
        uint8_t allowed;
        bool door_open;
    } params;
    union {
        int progress;
        lv_scr_load_anim_t anim;
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

BaseType_t display_net_status(net_status_t status, char* buf)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_NET_STATUS;
    evt.params.net_status = status;
    strncpy(evt.buf, buf, DISPLAY_EVT_BUF_SIZE);

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


BaseType_t display_power_status(power_status_t status)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_POWER_STATUS;
    evt.params.power_status = status;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif


BaseType_t display_acl_status(acl_status_t status, int progress)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_ACL_STATUS;
    evt.params.acl_status = status;
    evt.extparams.progress = progress;
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


BaseType_t display_door_state(bool door_open)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_DOOR_STATE;
    evt.params.door_open = door_open;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}
#else
{ return -1; }
#endif


BaseType_t display_show_screen(screen_t screen, lv_scr_load_anim_t anim)
#ifdef DISPLAY_ENABLED
{
    display_evt_t evt;
    evt.params.screen = screen;
    evt.extparams.anim = anim;
    evt.cmd = DISP_CMD_SHOW_SCREEN;

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

    display_lvgl_init_scr();
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

    lv_obj_t *scr_blank = ui_blank_create();
    lv_obj_t *scr_idle = ui_idle_create();
    lv_obj_t *scr_access = ui_access_create();
    lv_obj_t *scr_info = ui_info_create();

    esp_task_wdt_add(NULL);

    while(1) {
        display_evt_t evt;

        button = gpio_get_level(GPIO_PIN_FP_BUTTON);

        if (button != last_button) {
            ESP_LOGD(TAG, "Button now=%d", button);
            if (button == 0) {
              main_task_event(MAIN_EVT_UI_BUTTON_PRESS);
            }
        }

        last_button = button;

        esp_task_wdt_reset();

        display_lvgl_periodic();

        if (xQueueReceive(m_q, &evt, (10 / portTICK_PERIOD_MS)) == pdPASS) {
            switch(evt.cmd) {
            case DISP_CMD_WIFI_STATUS:
                ui_idle_set_wifi_status(evt.params.wifi_status);
                break;
            case DISP_CMD_WIFI_RSSI:
                ui_idle_set_rssi(evt.params.rssi);
                break;
            case DISP_CMD_NET_STATUS:
                ui_info_set_status(evt.params.net_status, evt.buf);
                break;
            case DISP_CMD_ACL_STATUS:
                ui_idle_set_acl_status(evt.params.acl_status);
                if (evt.params.acl_status == ACL_STATUS_DOWNLOADING)
                  ui_idle_set_acl_download_progress(evt.extparams.progress);
                break;
            case DISP_CMD_MQTT_STATUS:
                ui_idle_set_mqtt_status(evt.params.mqtt_status);
                break;
            case DISP_CMD_POWER_STATUS:
                ui_idle_set_power_status(evt.params.power_status);
                break;
            case DISP_CMD_ALLOWED_MSG:
                ui_access_set_user(evt.buf, evt.params.allowed);
                break;
            case DISP_CMD_DOOR_STATE:
                ui_idle_set_door_state(evt.params.door_open);
                break;
            case DISP_CMD_SHOW_SCREEN:
                switch (evt.params.screen) {
                  case SCREEN_BLANK:
                    if (s_scr != scr_blank) {
                      lv_scr_load_anim(scr_blank, evt.extparams.anim, 500, 0, false);
                      s_scr = scr_blank;
                    }
                    break;
                  case SCREEN_SPLASH:
                    if (s_scr != scr_splash) {
                      lv_scr_load_anim(scr_splash, evt.extparams.anim, 500, 0, false);
                      s_scr = scr_splash;
                      ui_splash_reset();
                    }
                    break;
                  case SCREEN_IDLE:
                    if (s_scr != scr_idle) {
                      lv_scr_load_anim(scr_idle, evt.extparams.anim, 500, 0, false);
                      s_scr = scr_idle;
                    }
                    break;
                  case SCREEN_ACCESS:
                    if (s_scr != scr_access) {
                      lv_scr_load_anim(scr_access, evt.extparams.anim, 500, 0, false);
                      s_scr = scr_access;
                    }
                    break;
                  case SCREEN_INFO:
                    if (lv_scr_act() != scr_info) {
                      lv_scr_load_anim(scr_info, evt.extparams.anim, 500, 0, false);
                      s_scr = scr_info;
                    }
                    break;
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
            strftime(strftime_buf, sizeof(strftime_buf), "%l:%M", &timeinfo);

            ui_idle_set_time(strftime_buf);

            // strftime_buf is time
            last_heartbeat_tick = now;
        }
    }
}
#else
{  }
#endif
