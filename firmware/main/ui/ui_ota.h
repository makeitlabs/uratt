#ifndef _UI_OTA_H
#define _UI_OTA_H

#include "display_task.h"

lv_obj_t* ui_ota_create(void);

void ui_ota_set_status(ota_status_t status);
void ui_ota_set_download_progress(int percent);

#endif
