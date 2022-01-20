#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

lv_obj_t* ui_blank_create(void)
{
  lv_obj_t* scr = lv_obj_create(NULL);

  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_radius(&style, 0);
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_outline_width(&style, 0);
  lv_style_set_outline_pad(&style, 0);
  lv_style_set_border_width(&style, 0);
  lv_style_set_pad_all(&style, 0);
  lv_style_set_pad_row(&style, 1);
  lv_style_set_pad_column(&style, 1);

  lv_obj_add_style(scr, &style, 0);

  return scr;
}
