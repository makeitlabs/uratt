#include <stdio.h>
#include <math.h>
#include "lvgl.h"
#include "display_task.h"

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


static lv_obj_t *label_wifi = NULL;
static lv_obj_t *label_acl = NULL;
static lv_obj_t *label_mqtt = NULL;
static lv_obj_t *label_battery = NULL;

static lv_obj_t *label_time = NULL;

static lv_obj_t *label_scan = NULL;

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
        mqtt_blink = 32;
      } else {
        mqtt_blink--;
        if (mqtt_blink == 0) {
          mqtt_status = MQTT_STATUS_CONNECTED;
        }
      }
      lv_obj_set_style_opa(label_mqtt, (count < 7) ? 255 : 64, 0);
    } else {
      lv_obj_set_style_opa(label_mqtt, 255, 0);
    }

    if (wifi_status == WIFI_STATUS_ERROR) {
        lv_label_set_text_static(label_wifi, (count < 7) ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);
    } else if (wifi_status == WIFI_STATUS_DISCONNECTED) {
        lv_label_set_text_static(label_wifi, (count < 7) ? LV_SYMBOL_WIFI : " ");
    } else if (wifi_status == WIFI_STATUS_CONNECTING) {
        lv_label_set_text_static(label_wifi, (count < 7) ? LV_SYMBOL_WIFI : LV_SYMBOL_REFRESH);
    } else {
        lv_label_set_text_static(label_wifi, LV_SYMBOL_WIFI);
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
    case MQTT_STATUS_DATA_SENT:
    case MQTT_STATUS_DATA_RECEIVED:
      color = lv_palette_main(LV_PALETTE_GREEN);
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
      color = lv_palette_main(LV_PALETTE_GREY);
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
  lv_obj_set_style_text_color(label_wifi, color, 0);
  wifi_status = status;
}


void ui_idle_set_rssi(int rssi)
{
  if (wifi_status == WIFI_STATUS_CONNECTED) {
    lv_color_t color = lv_palette_main(LV_PALETTE_GREY);

    if (rssi >= -55) {
      color = lv_palette_main(LV_PALETTE_LIGHT_GREEN);
    } else if (rssi >= -65) {
      color = lv_palette_main(LV_PALETTE_YELLOW);
    } else if (rssi >= -75) {
      color = lv_palette_main(LV_PALETTE_AMBER);
    } else if (rssi >= -85) {
      color = lv_palette_main(LV_PALETTE_ORANGE);
    } else if (rssi >= -90) {
      color = lv_palette_main(LV_PALETTE_RED);
    }

    lv_obj_set_style_text_color(label_wifi, color, 0);
  }
}

void ui_idle_set_time(char *time_str)
{
  if (label_time) {
    lv_label_set_text(label_time, time_str);
  }
}


#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 20

lv_obj_t* ui_idle_create()
{
  lv_obj_t* scr = lv_obj_create(NULL);

  static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT)];

  lv_obj_t * canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_canvas_fill_bg(canvas, lv_color_make(32,32,48), LV_OPA_COVER);

  label_battery = lv_label_create(canvas);
  lv_obj_set_width(label_battery, 22);
  lv_label_set_text_static(label_battery, LV_SYMBOL_CHARGE);
  lv_obj_align(label_battery, LV_ALIGN_TOP_LEFT, 2, 0);
  lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_battery, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

  label_wifi = lv_label_create(canvas);
  lv_obj_set_width(label_wifi, 20);
  lv_label_set_text_static(label_wifi, LV_SYMBOL_WIFI);
  lv_obj_align_to(label_wifi, label_battery, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_wifi, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

  label_acl = lv_label_create(canvas);
  lv_obj_set_width(label_acl, 20);
  lv_label_set_text_static(label_acl, LV_SYMBOL_DOWNLOAD);
  lv_obj_align_to(label_acl, label_wifi, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
  lv_obj_set_style_text_font(label_acl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_acl, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);

  label_mqtt = lv_label_create(canvas);
  lv_obj_set_width(label_mqtt, 20);
  lv_label_set_text_static(label_mqtt, LV_SYMBOL_SHUFFLE);
  lv_obj_align_to(label_mqtt, label_acl, LV_ALIGN_OUT_RIGHT_MID, 2, -1);
  lv_obj_set_style_text_font(label_mqtt, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label_mqtt, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);


  label_time = lv_label_create(canvas);
  lv_obj_set_width(label_time, 60);
  lv_obj_align(label_time, LV_ALIGN_TOP_RIGHT, -2, 0);
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label_time, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);


  label_scan = lv_label_create(scr);
  lv_label_set_long_mode(label_scan, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label_scan, 156);
  lv_label_set_text_static(label_scan, "SCAN RFID TAG BELOW                   ");
  lv_obj_align(label_scan, LV_ALIGN_BOTTOM_LEFT, 0, -10);
  lv_obj_set_style_text_font(label_scan, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(label_scan, lv_palette_main(LV_PALETTE_BLUE), 0);


  my_tim_ctx.scr = scr;
  timer = lv_timer_create(anim_timer_cb, 20, &my_tim_ctx);


  return scr;
}
