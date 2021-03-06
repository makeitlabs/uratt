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

#ifndef _NET_TASK_H
#define _NET_TASK_H

#include "display_task.h"

void net_init(void);
void net_task(void *pvParameters);

esp_err_t net_connect(void);
esp_err_t net_disconnect(void);

esp_err_t net_cmd_queue(int cmd);
esp_err_t net_cmd_queue_access(char *member, int allowed);
esp_err_t net_cmd_queue_access_error(char *err, char *err_ext);
esp_err_t net_cmd_queue_wget(char *url, char *filename);
esp_err_t net_cmd_queue_power_status(power_status_t status);
esp_err_t net_cmd_queue_door_state(bool door_open);

typedef enum  {
    NET_CMD_INIT = 0,
    NET_CMD_DISCONNECT,
    NET_CMD_CONNECT,
    NET_CMD_DOWNLOAD_ACL,
    NET_CMD_SEND_ACL_UPDATED,
    NET_CMD_SEND_ACL_FAILED,
    NET_CMD_SEND_WIFI_STR,
    NET_CMD_SEND_ACCESS,
    NET_CMD_SEND_ACCESS_ERROR,
    NET_CMD_SEND_POWER_STATUS,
    NET_CMD_SEND_DOOR_STATE,
    NET_CMD_OTA_UPDATE,
    NET_CMD_WGET
} net_cmd_t;

extern uint8_t g_mac_addr[6];

#endif
