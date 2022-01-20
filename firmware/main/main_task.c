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
#include <stdlib.h>
#include <esp_log.h>
#include <esp_system.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "system.h"
#include "esp_task_wdt.h"

#include "system_task.h"
#include "beep_task.h"
#include "door_task.h"
#include "rfid_task.h"
#include "display_task.h"
#include "net_task.h"
#include "main_task.h"

static const char *TAG = "main_task";

typedef enum {
  STATE_INVALID = 0,
  STATE_INIT,
  STATE_INITIAL_LOCK,
  STATE_WAIT_READ,
  STATE_START_RFID_READ,
  STATE_WAIT_RFID,
  STATE_RFID_VALID,
  STATE_RFID_INVALID,
  STATE_UNLOCKED,
  STATE_UNLOCKED_OPEN,
  STATE_LOCK,
  STATE_GO_TO_SLEEP,
  STATE_PRE_SLEEP1,
  STATE_PRE_SLEEP2,
  STATE_SLEEPING,
  STATE_WAKE_UP,
  STATE_WAKING,
  STATE_SHOW_INFO,
  STATE_SHOWING_INFO
} main_state_t;

static const char* state_names[] =
  { "STATE_INVALID",
    "STATE_INIT",
    "STATE_INITIAL_LOCK",
    "STATE_WAIT_READ",
    "STATE_START_RFID_READ",
    "STATE_WAIT_RFID",
    "STATE_RFID_VALID",
    "STATE_RFID_INVALID",
    "STATE_UNLOCKED",
    "STATE_UNLOCKED_OPEN",
    "STATE_LOCK",
    "STATE_GO_TO_SLEEP",
    "STATE_PRE_SLEEP1",
    "STATE_PRE_SLEEP2",
    "STATE_SLEEPING",
    "STATE_WAKE_UP",
    "STATE_WAKING",
    "STATE_SHOW_INFO",
    "STATE_SHOWING_INFO",
    "!!!UPDATE_STATE_NAMES_TABLE!!!",
    "!!!UPDATE_STATE_NAMES_TABLE!!!",
    "!!!UPDATE_STATE_NAMES_TABLE!!!",
    "!!!UPDATE_STATE_NAMES_TABLE!!!",
  };

typedef struct main_evt {
    main_evt_id_t id;
} main_evt_t;

#define MAIN_QUEUE_DEPTH 8

static QueueHandle_t m_q;

void main_task_init(void)
{
  ESP_LOGI(TAG, "task init");
  m_q = xQueueCreate(MAIN_QUEUE_DEPTH, sizeof(main_evt_t));
  if (m_q == NULL) {
    ESP_LOGE(TAG, "FATAL: Cannot create main task queue!");
  }
}


BaseType_t main_task_event(main_evt_id_t e)
{
    main_evt_t evt;
    evt.id = e;

    return xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

void main_task_timer_cb(TimerHandle_t timer)
{
  main_evt_t evt;
  evt.id = MAIN_EVT_TIMER_EXPIRED;
  xQueueSendToBack(m_q, &evt, 250 / portTICK_PERIOD_MS);
}

void main_task(void *pvParameters)
{
  static member_record_t active_member_record;
  static main_state_t state = STATE_INIT, last_state = STATE_INVALID;

  TimerHandle_t timer = xTimerCreate("smtimer", 1000, pdFALSE, (void*)0,  main_task_timer_cb);

  bool power_ok = true;
  bool net_connected = false;
  bool door_open = false;

  esp_task_wdt_add(NULL);

  ESP_LOGI(TAG, "task start, TICK_RATE_HZ=%u", configTICK_RATE_HZ);
  while(1) {
    main_evt_t evt;
    evt.id = MAIN_EVT_NONE;

    esp_task_wdt_reset();

    if (xQueueReceive(m_q, &evt, (20 / portTICK_PERIOD_MS)) == pdTRUE) {
      // handle some events immediately, regardless of system state
      switch(evt.id) {
        case MAIN_EVT_POWER_LOSS:
          ESP_LOGE(TAG, "MAIN_EVT_POWER_LOSS");
          display_power_status(POWER_STATUS_ON_BATT);
          net_cmd_queue_power_status(POWER_STATUS_ON_BATT);
          power_ok = false;
          break;
        case MAIN_EVT_POWER_RESTORED:
          ESP_LOGE(TAG, "MAIN_EVT_POWER_RESTORED");
          display_power_status(POWER_STATUS_ON_EXT);
          net_cmd_queue_power_status(POWER_STATUS_ON_EXT);
          power_ok = true;
          break;
        case MAIN_EVT_NET_CONNECT:
          net_connected = true;
          break;
        case MAIN_EVT_NET_DISCONNECT:
          net_connected = false;
          break;
        case MAIN_EVT_ALARM_DOOR_OPEN:
          door_open = true;
          net_cmd_queue_door_state(true);
          display_door_state(true);
          beep_queue(_beep_door_open);
          ESP_LOGI(TAG, "Door opened");
          break;
        case MAIN_EVT_ALARM_DOOR_CLOSED:
          door_open = false;
          net_cmd_queue_door_state(false);
          display_door_state(false);
          beep_queue(_beep_door_closed);
          ESP_LOGI(TAG, "Door closed");
          break;
        case MAIN_EVT_UI_BUTTON_PRESS:
          beep_queue(_beep_button_press);
          break;
        default:
          break;
      }
    }

    if (state != last_state) {
      ESP_LOGI(TAG, "State change: %s -> %s", state_names[last_state], state_names[state]);
    }
    last_state = state;

    switch(state) {
    case STATE_INIT:
      display_show_screen(SCREEN_SPLASH, LV_SCR_LOAD_ANIM_NONE);
      beep_queue(_beep_init);

      state = STATE_INITIAL_LOCK;
      break;

    case STATE_INITIAL_LOCK:
      door_lock();
      net_cmd_queue(NET_CMD_CONNECT);
      xTimerChangePeriod(timer, 3000 / portTICK_PERIOD_MS, 0);
      xTimerStart(timer, 0);
      state = STATE_WAIT_READ;
      break;

    case STATE_WAIT_READ:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED)
        state = STATE_START_RFID_READ;
      break;

    case STATE_START_RFID_READ:
      display_show_screen(SCREEN_IDLE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
      state = STATE_WAIT_RFID;
      break;

    case STATE_WAIT_RFID:
      if (power_ok) {
        if (xTimerIsTimerActive(timer)) {
          ESP_LOGW(TAG, "main power now ok");
          xTimerStop(timer, 0);
        }
      } else {
        if (!xTimerIsTimerActive(timer)) {
          ESP_LOGW(TAG, "main power lost, sleep in 15 seconds");
          xTimerChangePeriod(timer, 15000 / portTICK_PERIOD_MS, 0);
          xTimerStart(timer, 0);
        }
      }

      switch (evt.id) {
        case MAIN_EVT_RFID_PRE_SCAN:
          beep_queue(_beep_pre_scan);
          break;
        case MAIN_EVT_VALID_RFID_SCAN:
          display_show_screen(SCREEN_ACCESS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
          state = STATE_RFID_VALID;
          break;
        case MAIN_EVT_INVALID_RFID_SCAN:
          xTimerStop(timer, 0);
          display_show_screen(SCREEN_ACCESS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
          state = STATE_RFID_INVALID;
          break;

        case MAIN_EVT_TIMER_EXPIRED:
          state = STATE_GO_TO_SLEEP;
          break;

        case MAIN_EVT_UI_BUTTON_PRESS:
          state = STATE_SHOW_INFO;
          break;

        default:
          break;
      }
      break;

    case STATE_RFID_VALID:
      {
        rfid_get_member_record(&active_member_record);

        display_allowed_msg(active_member_record.name, active_member_record.allowed);

        if (active_member_record.allowed) {
          ESP_LOGI(TAG, "MEMBER ALLOWED");
          beep_queue(_beep_allowed);
          door_unlock();

          xTimerChangePeriod(timer, 7000 / portTICK_PERIOD_MS, 0);
          xTimerStart(timer, 0);

          state = STATE_UNLOCKED;
        } else {
          ESP_LOGI(TAG, "MEMBER DENIED");
          beep_queue(_beep_denied);

          xTimerChangePeriod(timer, 10000 / portTICK_PERIOD_MS, 0);
          xTimerStart(timer, 0);

          state = STATE_WAIT_READ;
        }

        net_cmd_queue_access(active_member_record.name, active_member_record.allowed);
      }
      break;

    case STATE_RFID_INVALID:
      {
        rfid_get_member_record(&active_member_record);

        display_allowed_msg("Unknown RFID", false);

        beep_queue(_beep_invalid);

        char tagstr[12];
        snprintf(tagstr, sizeof(tagstr), "%10.10u", active_member_record.tag);
        net_cmd_queue_access_error("unknown rfid tag", tagstr);

        xTimerChangePeriod(timer, 8000 / portTICK_PERIOD_MS, 0);
        xTimerStart(timer, 0);

        state = STATE_WAIT_READ;
      }
      break;

    case STATE_UNLOCKED:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED) {
        state = STATE_LOCK;
      } else if (door_open) {
        xTimerChangePeriod(timer, 1000 / portTICK_PERIOD_MS, 0);
        xTimerStart(timer, 0);
        state = STATE_UNLOCKED_OPEN;
      }
      break;

    case STATE_UNLOCKED_OPEN:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED) {
        state = STATE_LOCK;
      }
      break;

    case STATE_LOCK:
      beep_queue(_beep_lock);
      door_lock();

      xTimerChangePeriod(timer, 10000 / portTICK_PERIOD_MS, 0);
      xTimerStart(timer, 0);

      state = STATE_WAIT_READ;
      break;

    case STATE_GO_TO_SLEEP:
      display_power_status(POWER_STATUS_SLEEP);
      net_cmd_queue_power_status(POWER_STATUS_SLEEP);
      xTimerChangePeriod(timer, 2000 / portTICK_PERIOD_MS, 0);
      xTimerStart(timer, 0);
      state = STATE_PRE_SLEEP1;
      break;

    case STATE_PRE_SLEEP1:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED) {
        display_show_screen(SCREEN_BLANK, LV_SCR_LOAD_ANIM_FADE_ON);
        net_cmd_queue(NET_CMD_DISCONNECT);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        xTimerChangePeriod(timer, 1000 / portTICK_PERIOD_MS, 0);
        xTimerStart(timer, 0);
        system_pre_sleep();
        state = STATE_PRE_SLEEP2;
      }
      break;

    case STATE_PRE_SLEEP2:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED && !net_connected) {
        state = STATE_SLEEPING;
      }
      break;

    case STATE_SLEEPING:
      system_sleep();
      // zzzz...
      state = STATE_WAKE_UP;
      break;

    case STATE_WAKE_UP:
      system_wake();
      display_show_screen(SCREEN_SPLASH, LV_SCR_LOAD_ANIM_FADE_ON);

      vTaskDelay(1000/portTICK_PERIOD_MS);

      net_cmd_queue(NET_CMD_CONNECT);
      net_cmd_queue_power_status(POWER_STATUS_WAKE);
      if (!power_ok) {
        // likely woke up from button or timer
        display_power_status(POWER_STATUS_ON_BATT);
        net_cmd_queue_power_status(POWER_STATUS_ON_BATT);
      }
      state = STATE_WAKING;
      break;

    case STATE_WAKING:
      if (net_connected) {
        display_show_screen(SCREEN_IDLE, LV_SCR_LOAD_ANIM_FADE_ON);
        state = STATE_START_RFID_READ;
      }
      break;

    case STATE_SHOW_INFO:
      xTimerChangePeriod(timer, 10000 / portTICK_PERIOD_MS, 0);
      xTimerStart(timer, 0);

      display_show_screen(SCREEN_INFO, LV_SCR_LOAD_ANIM_MOVE_TOP);

      state = STATE_SHOWING_INFO;
      break;

    case STATE_SHOWING_INFO:
      if (evt.id == MAIN_EVT_TIMER_EXPIRED || evt.id == MAIN_EVT_UI_BUTTON_PRESS) {
        display_show_screen(SCREEN_IDLE, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
        state = STATE_START_RFID_READ;
      }

      break;

    default:
      break;
    }
  }
}
