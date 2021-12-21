#include <math.h>
#include "lvgl.h"
#include "display_task.h"


static lv_style_t style;

static lv_obj_t *label_user = NULL;


void ui_access_set_user(char *user, bool allowed)
{
  lv_color_t color = allowed ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED);
  lv_style_set_bg_color(&style, color);
  lv_obj_add_style(label_user->parent, &style, 0);

  lv_label_set_text_fmt(label_user, "%s %s", allowed ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE, user);
}


lv_obj_t* ui_access_create()
{
  lv_obj_t* scr = lv_obj_create(NULL);

  lv_style_init(&style);
  lv_style_set_radius(&style, 0);
  lv_style_set_bg_color(&style, lv_color_black());

  lv_obj_add_style(scr, &style, 0);

  label_user = lv_label_create(scr);
  lv_label_set_long_mode(label_user, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label_user, 156);
  lv_obj_align(label_user, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(label_user, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(label_user, lv_color_white(), 0);

  return scr;
}
