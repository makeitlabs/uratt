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
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANT
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 --------------------------------------------------------------------------
 Author: Steve Richardson (steve.richardson@makeitlabs.com)
 -------------------------------------------------------------------------- */

#ifndef _MAIN_TASK_H
#define _MAIN_TASK_H

typedef enum {
  MAIN_EVT_NONE = 0,
  MAIN_EVT_OTA_UPDATE,
  MAIN_EVT_OTA_UPDATE_SUCCESS,
  MAIN_EVT_OTA_UPDATE_FAILED,
  MAIN_EVT_NET_CONNECT,
  MAIN_EVT_NET_DISCONNECT,
  MAIN_EVT_BATTERY_LOW,
  MAIN_EVT_BATTERY_OK,
  MAIN_EVT_POWER_LOSS,
  MAIN_EVT_POWER_RESTORED,
  MAIN_EVT_TIMER_EXPIRED,
  MAIN_EVT_RFID_PRE_SCAN,
  MAIN_EVT_VALID_RFID_SCAN,
  MAIN_EVT_INVALID_RFID_SCAN,
  MAIN_EVT_ALARM_DOOR_OPEN,
  MAIN_EVT_ALARM_DOOR_CLOSED,
  MAIN_EVT_UI_BUTTON_PRESS,
} main_evt_id_t;

void main_task(void *pvParameters);
void main_task_init();
BaseType_t main_task_event(main_evt_id_t e);

#endif
