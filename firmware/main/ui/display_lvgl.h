#ifndef _DISPLAY_LVGL_H
#define _DISPLAY_LVGL_H

#include "lvgl.h"

lv_obj_t *display_lvgl_init_scr(void);
void display_lvgl_periodic(void);

void display_lvgl_disp_off(bool off);



#endif
