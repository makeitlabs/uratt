#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include <math.h>
#include "display_task.h"
#include "esp_ota_ops.h"

static lv_style_t style;
lv_obj_t* label_mac;
lv_obj_t* label_ip;

void ui_info_set_status(net_status_t status, char* buf)
{
  switch (status) {
    case NET_STATUS_CUR_MAC:
      lv_label_set_text(label_mac, buf);
      break;
    case NET_STATUS_CUR_IP:
      lv_label_set_text(label_ip, buf);
      break;
  }
}

lv_obj_t* ui_info_create()
{
  lv_obj_t* scr = lv_obj_create(NULL);

  lv_style_init(&style);
  lv_style_set_radius(&style, 0);
  lv_style_set_bg_color(&style, lv_color_white());

  lv_obj_add_style(scr, &style, 0);


  static lv_coord_t col_dsc[] = {40, 120, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {20, 20, 20, 20, LV_GRID_TEMPLATE_LAST};

  lv_obj_t * grid = lv_obj_create(scr);
  lv_obj_remove_style_all(grid);
  lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
  lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);
  lv_obj_set_size(grid, 160, 80);
  lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);


  lv_obj_t* l = lv_label_create(grid);
  lv_obj_set_width(l, 40);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);
  lv_label_set_text(l, "MAC");

  label_mac = lv_label_create(grid);
  lv_obj_set_width(label_mac, 120);
  lv_obj_set_grid_cell(label_mac, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_style_text_font(label_mac, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(label_mac, lv_color_black(), 0);

  l = lv_label_create(grid);
  lv_obj_set_width(l, 40);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);
  lv_label_set_text(l, "IP");

  label_ip = lv_label_create(grid);
  lv_obj_set_width(label_ip, 120);
  lv_obj_set_grid_cell(label_ip, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_style_text_font(label_ip, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(label_ip, lv_color_black(), 0);

  l = lv_label_create(grid);
  lv_obj_set_width(l, 40);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);
  lv_label_set_text(l, "Ver");

  l = lv_label_create(grid);
  lv_obj_set_width(l, 120);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);

  const esp_app_desc_t* desc = esp_ota_get_app_description();
  lv_label_set_text(l, desc->version);

  l = lv_label_create(grid);
  lv_obj_set_width(l, 40);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 3, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);
  lv_label_set_text(l, "");

  l = lv_label_create(grid);
  lv_obj_set_width(l, 120);
  lv_obj_set_grid_cell(l, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_black(), 0);

  char s[32];
  snprintf(s, 32, "%s %s", desc->date, desc->time);
  lv_label_set_text(l, s);

  return scr;
}
