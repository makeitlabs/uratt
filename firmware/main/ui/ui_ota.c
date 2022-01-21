#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include <stdio.h>
#include <math.h>
#include "display_task.h"
#include "ui_ota.h"

static lv_obj_t *label_status = NULL;
static lv_obj_t *bar_progress = NULL;
static lv_obj_t *label_progress = NULL;

static ota_status_t ota_status = OTA_STATUS_INIT;

void ui_ota_set_status(ota_status_t status)
{
    switch(status) {
        case OTA_STATUS_INIT:
            break;
        case OTA_STATUS_ERROR:
            break;
        case OTA_STATUS_DOWNLOADING:
            break;
        default:
        break;
    }
    ota_status = status;
}


void ui_ota_set_download_progress(int percent)
{
  lv_label_set_text_fmt(label_progress, "%d%%", percent);
  lv_bar_set_value(bar_progress, percent, LV_ANIM_OFF);
}


lv_obj_t* ui_ota_create(void)
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

    static lv_style_t gstyle;
    lv_style_init(&gstyle);
    lv_style_set_bg_color(&gstyle, lv_color_black());
    lv_style_set_radius(&gstyle, 5);
    lv_style_set_outline_width(&gstyle, 0);
    lv_style_set_outline_pad(&gstyle, 0);
    lv_style_set_border_width(&gstyle, 0);
    lv_style_set_pad_all(&gstyle, 0);

    static lv_coord_t col_dsc[] = {160, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {40, 40, LV_GRID_TEMPLATE_LAST};

    lv_obj_t * grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);
    lv_obj_set_size(grid, 160, 80);
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    lv_obj_add_style(grid, &style, 0);

    label_status = lv_label_create(grid);
    lv_obj_set_grid_cell(label_status, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_status, 156);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_28, 0);
    lv_label_set_text_static(label_status, "Updating Firmware... Please Wait");
    lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
    lv_obj_center(label_status);

    static lv_style_t style_bg;
    static lv_style_t style_indic;

    lv_style_init(&style_bg);
    lv_style_set_border_color(&style_bg, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_border_width(&style_bg, 2);
    lv_style_set_pad_all(&style_bg, 2);
    lv_style_set_radius(&style_bg, 0);

    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_radius(&style_indic, 0);

    bar_progress = lv_bar_create(grid);

    lv_bar_set_start_value(bar_progress, 100, LV_ANIM_OFF);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_mode(bar_progress, LV_BAR_MODE_NORMAL);
    lv_obj_remove_style_all(bar_progress);
    lv_obj_add_style(bar_progress, &style_bg, 0);
    lv_obj_add_style(bar_progress, &style_indic, LV_PART_INDICATOR);

    lv_obj_center(bar_progress);
    lv_obj_set_size(bar_progress, 140, 20);
    lv_obj_set_grid_cell(bar_progress, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    label_progress = lv_label_create(bar_progress);
    lv_label_set_long_mode(label_progress, LV_LABEL_LONG_CLIP);
    lv_obj_align(label_progress, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label_progress, &lv_font_montserrat_14, 0);
    lv_label_set_text_static(label_progress, "");
    lv_obj_set_style_text_color(label_progress, lv_color_white(), 0);

    return scr;
}
