#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include <stdio.h>

static lv_obj_t *label_count;

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 80
void ui_sleep_set_countdown(int count)
{
  char s[40];
  snprintf(s, sizeof(s), "Sleeping in... %d", count);
  lv_label_set_text(label_count, s);
}

lv_obj_t* ui_sleep_create()
{
  lv_obj_t* scr = lv_obj_create(NULL);

  lv_obj_t *label_sleep = lv_label_create(scr);
  lv_obj_set_width(label_sleep, 156);
  lv_obj_align(label_sleep, LV_ALIGN_CENTER, 0, -18);
  lv_label_set_text(label_sleep, LV_SYMBOL_BATTERY_3);
  lv_obj_set_style_text_font(label_sleep, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(label_sleep, lv_color_black(), 0);

  label_count = lv_label_create(scr);
  lv_obj_set_width(label_count, 156);
  lv_obj_align_to(label_count, label_sleep, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(label_count, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(label_count, lv_color_black(), 0);

  return scr;
}
