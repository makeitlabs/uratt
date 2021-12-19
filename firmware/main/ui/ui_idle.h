#ifndef _UI_IDLE_H
#define _UI_IDLE_H

#include "display_task.h"

lv_obj_t* ui_idle_create(void);

void ui_idle_set_rssi(int rssi);
void ui_idle_set_time(char *time_str);
void ui_idle_set_acl_status(acl_status_t status);


void ui_idle_set_status2(char *status);
void ui_idle_set_status3(char *status);

#endif
