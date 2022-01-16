#ifndef _UI_INFO_H
#define _UI_INFO_H

#include "display_task.h"

lv_obj_t* ui_info_create(void);
void ui_info_set_status(net_status_t status, char* buf);

#endif
