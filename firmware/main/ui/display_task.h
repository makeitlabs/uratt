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

#ifndef _DISPLAY_TASK
#define _DISPLAY_TASK

#include "freertos/FreeRTOS.h"

void display_task(void *pvParameters);
void display_init();

BaseType_t display_wifi_msg(char *msg);
BaseType_t display_wifi_rssi(int16_t rssi);
BaseType_t display_allowed_msg(char *msg, uint8_t allowed);
BaseType_t display_show_idle(uint16_t delay, bool destroy);
BaseType_t display_show_access();

typedef enum {
    ACL_STATUS_INIT,
    ACL_STATUS_ERROR,
    ACL_STATUS_DOWNLOADED_UPDATED,
    ACL_STATUS_DOWNLOADED_SAME_HASH,
    ACL_STATUS_CACHED,
    ACL_STATUS_DOWNLOADING,
    ACL_STATUS_DOWNLOAD_PROGRESS
} acl_status_t;

BaseType_t display_acl_status(acl_status_t status);

typedef enum {
  MQTT_STATUS_INIT,
  MQTT_STATUS_ERROR,
  MQTT_STATUS_DISCONNECTED,
  MQTT_STATUS_CONNECTED,
  MQTT_STATUS_DATA_RECEIVED,
  MQTT_STATUS_DATA_SENT
} mqtt_status_t;

BaseType_t display_mqtt_status(mqtt_status_t status);


typedef enum {
  WIFI_STATUS_INIT,
  WIFI_STATUS_ERROR,
  WIFI_STATUS_DISCONNECTED,
  WIFI_STATUS_CONNECTING,
  WIFI_STATUS_CONNECTED,
} wifi_status_t;

BaseType_t display_wifi_status(wifi_status_t status);


#endif
