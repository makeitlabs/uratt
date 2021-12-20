#include <math.h>
#include "lvgl.h"
#include "display_task.h"

typedef struct {
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

lv_obj_t *canvas = NULL;
static lv_obj_t *label_user = NULL;


void ui_access_set_user(char *user, bool allowed)
{
  lv_color_t color = allowed ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED);

  lv_label_set_text(label_user, user);
  lv_canvas_fill_bg(canvas, color, LV_OPA_COVER);
}

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 80

lv_obj_t* ui_access_create()
{
  lv_obj_t* scr = lv_obj_create(NULL);

  static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT)];

  canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

  label_user = lv_label_create(canvas);
  lv_label_set_long_mode(label_user, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label_user, 156);
  lv_obj_align(label_user, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(label_user, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(label_user, lv_color_white(), 0);

  return scr;
}
