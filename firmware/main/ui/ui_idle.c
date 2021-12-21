#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include <stdio.h>
#include <math.h>
#include "display_task.h"
#include "ui_idle.h"

LV_IMG_DECLARE(wifi_0);
LV_IMG_DECLARE(wifi_1);
LV_IMG_DECLARE(wifi_2);
LV_IMG_DECLARE(wifi_3);
LV_IMG_DECLARE(wifi_4);

typedef struct {
    lv_obj_t *scr;
    int count_val;
    bool count_dir;
    int mqtt_blink;
} my_timer_context_t;

static my_timer_context_t my_tim_ctx = {
    .count_val = 0,
    .count_dir = true,
    .mqtt_blink = 0
};


static lv_obj_t *img_wifi = NULL;
static lv_obj_t *label_acl = NULL;
static lv_obj_t *label_mqtt = NULL;
static lv_obj_t *label_battery = NULL;

static lv_obj_t *label_time = NULL;

//static lv_obj_t *label_scan = NULL;

static lv_timer_t *timer;

static acl_status_t acl_status = ACL_STATUS_ERROR;
static mqtt_status_t mqtt_status = MQTT_STATUS_DISCONNECTED;
static wifi_status_t wifi_status = WIFI_STATUS_DISCONNECTED;

static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *) timer->user_data;
    int count = timer_ctx->count_val;
    bool count_dir = timer_ctx->count_dir;
    int mqtt_blink = timer_ctx->mqtt_blink;
    //lv_obj_t *scr = timer_ctx->scr;

    if (acl_status == ACL_STATUS_DOWNLOADING) {
        lv_obj_set_style_opa(label_acl, count * 16, 0);
    } else {
        lv_obj_set_style_opa(label_acl, 255, 0);
    }

    if (mqtt_status == MQTT_STATUS_DATA_SENT || mqtt_status == MQTT_STATUS_DATA_RECEIVED) {
        if (mqtt_blink == 0) {
            mqtt_blink = 16;
        } else {
            mqtt_blink--;
            if (mqtt_blink == 0) {
                ui_idle_set_mqtt_status(MQTT_STATUS_CONNECTED);
            }
        }
        lv_obj_set_style_opa(label_mqtt, (count % 2) == 0 ? 255 : 64, 0);
    } else {
        lv_obj_set_style_opa(label_mqtt, 255, 0);
    }
    if (wifi_status == WIFI_STATUS_ERROR) {
        if (count < 7) {
          lv_img_set_src(img_wifi, LV_SYMBOL_DUMMY LV_SYMBOL_WARNING);
        } else {
          lv_img_set_src(img_wifi, &wifi_0);
        }
    } else if (wifi_status == WIFI_STATUS_DISCONNECTED) {
      if (count < 7) {
        lv_img_set_src(img_wifi, LV_SYMBOL_DUMMY LV_SYMBOL_CLOSE);
      } else {
        lv_img_set_src(img_wifi, &wifi_0);
      }
    } else if (wifi_status == WIFI_STATUS_CONNECTING) {
      if (count < 7) {
        lv_img_set_src(img_wifi, LV_SYMBOL_DUMMY LV_SYMBOL_REFRESH);
      } else {
        lv_img_set_src(img_wifi, &wifi_0);
      }
    }

    if (count_dir) {
        count++;
        if (count >= 15)
        count_dir = !count_dir;
    } else {
        count--;
        if (count <= 0)
        count_dir = !count_dir;
    }

    timer_ctx->count_val = count;
    timer_ctx->count_dir = count_dir;
    timer_ctx->mqtt_blink = mqtt_blink;
}


void ui_idle_set_acl_status(acl_status_t status)
{
    lv_color_t color = lv_palette_main(LV_PALETTE_GREY);
    switch(status) {
        case ACL_STATUS_INIT:
        color = lv_palette_main(LV_PALETTE_GREY);
        break;
        case ACL_STATUS_ERROR:
        color = lv_palette_lighten(LV_PALETTE_RED, 2);
        break;
        case ACL_STATUS_DOWNLOADING:
        color = lv_palette_main(LV_PALETTE_CYAN);
        break;
        case ACL_STATUS_DOWNLOADED_UPDATED:
        case ACL_STATUS_DOWNLOADED_SAME_HASH:
        color = lv_palette_main(LV_PALETTE_GREEN);
        break;
        case ACL_STATUS_CACHED:
        color = lv_palette_main(LV_PALETTE_AMBER);
        break;
        default:
        break;
    }
    lv_obj_set_style_text_color(label_acl, color, 0);
    acl_status = status;
}

void ui_idle_set_mqtt_status(mqtt_status_t status)
{
    lv_color_t color = lv_palette_main(LV_PALETTE_GREY);
    switch(status) {
        case MQTT_STATUS_INIT:
        color = lv_palette_main(LV_PALETTE_GREY);
        break;
        case MQTT_STATUS_ERROR:
        color = lv_palette_lighten(LV_PALETTE_RED, 2);
        break;
        case MQTT_STATUS_DISCONNECTED:
        color = lv_palette_main(LV_PALETTE_GREY);
        break;
        case MQTT_STATUS_CONNECTED:
        color = lv_palette_main(LV_PALETTE_GREEN);
        break;

        case MQTT_STATUS_DATA_SENT:
        case MQTT_STATUS_DATA_RECEIVED:
          color = lv_palette_main(LV_PALETTE_CYAN);
        break;

        default:
        break;
    }
    lv_obj_set_style_text_color(label_mqtt, color, 0);
    mqtt_status = status;
}


void ui_idle_set_wifi_status(wifi_status_t status)
{
    lv_color_t color = lv_palette_main(LV_PALETTE_GREY);
    switch(status) {
        case WIFI_STATUS_INIT:
        color = lv_palette_main(LV_PALETTE_GREY);
        break;
        case WIFI_STATUS_ERROR:
        color = lv_palette_lighten(LV_PALETTE_RED, 2);
        break;
        case WIFI_STATUS_DISCONNECTED:
        color = lv_palette_lighten(LV_PALETTE_ORANGE, 1);
        break;
        case WIFI_STATUS_CONNECTING:
        color = lv_palette_main(LV_PALETTE_CYAN);
        break;
        case WIFI_STATUS_CONNECTED:
        color = lv_palette_main(LV_PALETTE_GREEN);
        break;
        default:
        break;
    }
    lv_obj_set_style_text_color(img_wifi, color, 0);
    wifi_status = status;
}


void ui_idle_set_rssi(int rssi)
{
    if (wifi_status == WIFI_STATUS_CONNECTED) {
        if (rssi >= -55) {
            lv_img_set_src(img_wifi, &wifi_4);
        } else if (rssi >= -65) {
            lv_img_set_src(img_wifi, &wifi_3);
        } else if (rssi >= -75) {
            lv_img_set_src(img_wifi, &wifi_2);
        } else if (rssi >= -85) {
            lv_img_set_src(img_wifi, &wifi_1);
        } else if (rssi >= -95) {
            lv_img_set_src(img_wifi, &wifi_0);
        }
    }
}

void ui_idle_set_time(char *time_str)
{
    if (label_time) {
        lv_label_set_text(label_time, time_str);
    }
}


lv_obj_t* make_grid_icon(lv_obj_t* grid, int row, int col, lv_style_t* style, char* symbol )
{
    lv_obj_t* c = lv_obj_create(grid);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_add_style(c, style, 0);
    lv_obj_set_align(c, LV_ALIGN_CENTER);
    lv_obj_t* o = lv_label_create(c);
    lv_obj_set_align(o, LV_ALIGN_CENTER);
    lv_label_set_text_static(o, symbol);
    lv_obj_set_style_text_font(o, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(o, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_center(o);
    return o;
}


lv_obj_t* ui_idle_create(void)
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
    //lv_style_set_border_color(&gstyle, lv_color_make(220,220,220));
    lv_style_set_pad_all(&gstyle, 0);
    /*
    lv_style_set_pad_left(&gstyle, 2);
    lv_style_set_pad_right(&gstyle, 2);
    lv_style_set_pad_top(&gstyle, 2);
    lv_style_set_pad_bottom(&gstyle, 2);
    lv_style_set_pad_row(&gstyle, 2);
    lv_style_set_pad_column(&gstyle, 2);
    */

    static lv_coord_t col_dsc[] = {40, 40, 40, 40, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {40, 40, LV_GRID_TEMPLATE_LAST};

    lv_obj_t * grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);
    lv_obj_set_size(grid, 160, 80);
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    lv_obj_add_style(grid, &style, 0);


    label_acl = make_grid_icon(grid, 1, 1, &gstyle, LV_SYMBOL_DOWNLOAD);
    label_mqtt = make_grid_icon(grid, 1, 2, &gstyle, LV_SYMBOL_SHUFFLE);
    label_battery = make_grid_icon(grid, 1, 3, &gstyle, LV_SYMBOL_CHARGE);


    lv_obj_t* wifi_cont = lv_obj_create(grid);
    lv_obj_set_grid_cell(wifi_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_size(wifi_cont, 30, 30);
    lv_obj_add_style(wifi_cont, &style, 0);
    img_wifi = lv_img_create(wifi_cont);
    lv_img_set_src(img_wifi, &wifi_0);
    lv_obj_align(img_wifi, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(img_wifi, &lv_font_montserrat_20, 0);


    label_time = lv_label_create(grid);
    lv_label_set_long_mode(label_time, LV_LABEL_LONG_CLIP);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_grid_cell(label_time, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_36, 0);
    lv_label_set_text_static(label_time, "12:00pm");
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);

    /*
    label_scan = lv_label_create(grid);
    lv_label_set_long_mode(label_scan, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_grid_cell(label_scan, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_label_set_text_static(label_scan, "12:00am      SCAN RFID BELOW     ");
    lv_obj_set_style_text_font(label_scan, &lv_font_montserrat_40, 0);
    //lv_obj_set_style_text_color(label_scan, lv_color_black(), 0);
    */

    my_tim_ctx.scr = scr;
    timer = lv_timer_create(anim_timer_cb, 100, &my_tim_ctx);


    return scr;
}
