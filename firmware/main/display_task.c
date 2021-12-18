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

static const char *TAG = "display_task";

static lv_obj_t *s_scr;

extern void lvgl_demo_ui(lv_obj_t *scr);

#define DISPLAY_QUEUE_DEPTH 8
#define DISPLAY_EVT_BUF_SIZE 32

typedef enum {
    DISP_CMD_WIFI_MSG = 0x00,
    DISP_CMD_WIFI_RSSI,
    DISP_CMD_NET_MSG,
    DISP_CMD_ALLOWED_MSG,
    DISP_CMD_USER_MSG,
    DISP_CMD_CLEAR_MSG,
    DISP_CMD_REDRAW_BG
} display_cmd_t;

typedef struct display_evt {
    display_cmd_t cmd;
    char buf[DISPLAY_EVT_BUF_SIZE];
    union {
        int16_t rssi;
        uint8_t allowed;
    } params;
} display_evt_t;

static QueueHandle_t m_q;

BaseType_t display_wifi_msg(char *msg)
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_WIFI_MSG;
    strncpy(evt.buf, msg, DISPLAY_EVT_BUF_SIZE);

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_wifi_rssi(int16_t rssi)
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_WIFI_RSSI;
    evt.params.rssi = rssi;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_net_msg(char *msg)
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_NET_MSG;
    strncpy(evt.buf, msg, DISPLAY_EVT_BUF_SIZE);

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_user_msg(char *msg)
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_USER_MSG;
    strncpy(evt.buf, msg, DISPLAY_EVT_BUF_SIZE);

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_allowed_msg(char *msg, uint8_t allowed)
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_ALLOWED_MSG;
    evt.params.allowed = allowed;
    strncpy(evt.buf, msg, DISPLAY_EVT_BUF_SIZE);

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_clear_msg()
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_CLEAR_MSG;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

BaseType_t display_redraw_bg()
{
    display_evt_t evt;
    evt.cmd = DISP_CMD_REDRAW_BG;
    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}


void display_init()
{

    gpio_set_direction(GPIO_PIN_FP_BUTTON, GPIO_MODE_INPUT);
    gpio_pullup_en(GPIO_PIN_FP_BUTTON);

    m_q = xQueueCreate(DISPLAY_QUEUE_DEPTH, sizeof(display_evt_t));
    if (m_q == NULL) {
        ESP_LOGE(TAG, "FATAL: Cannot create display queue!");
    }

}

void display_init_bg(void)
{
  // init bg
}

void display_task(void *pvParameters)
{
    portTickType last_heartbeat_tick;

    last_heartbeat_tick = xTaskGetTickCount();

    s_scr = display_lvgl_init_scr();

    display_init_bg();

    lvgl_demo_ui(s_scr);


    int button=0, last_button=0;
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
            case DISP_CMD_WIFI_MSG:
                // evt.buf is msg
                break;
            case DISP_CMD_WIFI_RSSI:
                // evt.params.rssi is int rssi
                break;
            case DISP_CMD_NET_MSG:
                // evt.buf is msg
                break;
            case DISP_CMD_USER_MSG:
                // evt.buf is msg
                break;
            case DISP_CMD_ALLOWED_MSG:
                // evt.buf is allowed msg
                // evt.params.allowed is true if allowed
                break;
            case DISP_CMD_CLEAR_MSG:
                // clear message
                break;
            case DISP_CMD_REDRAW_BG:
                // redraw bg
                break;
            }
        }

        portTickType now = xTaskGetTickCount();
        if (now - last_heartbeat_tick >= (500/portTICK_PERIOD_MS)) {
            // heartbeat

            char strftime_buf[64];
            time_t tnow;
            struct tm timeinfo;

            time(&tnow);
            setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
            tzset();
            localtime_r(&tnow, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

            // strftime_buf is time
            last_heartbeat_tick = now;
        }
    }
}
