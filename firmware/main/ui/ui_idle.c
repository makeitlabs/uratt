#include <math.h>
#include "lvgl.h"
#include "display_task.h"

typedef struct {
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

static lv_obj_t *label_wifi = NULL;
static lv_obj_t *label_acl = NULL;
static lv_obj_t *label_mqtt = NULL;
static lv_obj_t *label_battery = NULL;

static lv_obj_t *label_time = NULL;



static lv_obj_t *label2 = NULL;
static lv_obj_t *label3 = NULL;
static lv_obj_t *label4 = NULL;




void ui_idle_set_acl_status(acl_status_t status)
{
  lv_color_t color = lv_palette_main(LV_PALETTE_GREY);
  switch(status) {
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
}

void ui_idle_set_mqtt_status(mqtt_status_t status)
{
  lv_color_t color = lv_palette_main(LV_PALETTE_GREY);
  switch(status) {
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
}


void ui_idle_set_rssi(int rssi)
{
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

void ui_idle_set_time(char *time_str)
{
  if (label_time) {
    lv_label_set_text(label_time, time_str);
  }
}

void ui_idle_set_status2(char *status)
{
  if (label2) {
    lv_label_set_text(label2, status);
  }
}

void ui_idle_set_status3(char *status)
{
  if (label3) {
    lv_label_set_text(label3, status);
  }
}

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 16

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



  label2 = lv_label_create(scr);
  lv_label_set_long_mode(label2, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label2, 156);
  lv_obj_align_to(label2, canvas, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);
  lv_obj_set_style_text_font(label2, &lv_font_montserrat_14, 0);

  label3 = lv_label_create(scr);
  lv_label_set_long_mode(label3, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label3, 156);
  lv_obj_align_to(label3, label2, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);
  lv_obj_set_style_text_font(label3, &lv_font_montserrat_14, 0);

  label4 = lv_label_create(scr);
  lv_label_set_long_mode(label4, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label4, 156);
  lv_label_set_text_static(label4, "SCAN RFID TAG BELOW                   ");
  lv_obj_align(label4, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_text_font(label4, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label4, lv_palette_main(LV_PALETTE_BLUE), 0);

  return scr;
}
