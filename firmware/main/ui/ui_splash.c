#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include <math.h>

#ifndef PI
#define PI  (3.14159f)
#endif

LV_IMG_DECLARE(ratt_head)

typedef struct {
    lv_obj_t *scr;
    int count_val;
    bool flip;
} my_timer_context_t;

static lv_obj_t *arc[3];
static lv_obj_t *img_logo;
static lv_obj_t *label1;
static lv_obj_t *label2;

static lv_timer_t *timer;


static lv_color_t arc_color[] = {
    LV_COLOR_MAKE(232, 87, 116),
    LV_COLOR_MAKE(20, 232, 60),
    LV_COLOR_MAKE(90, 202, 228),
};



static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *) timer->user_data;
    bool flip = timer_ctx->flip;
    int count = timer_ctx->count_val;
    //lv_obj_t *scr = timer_ctx->scr;

    // Play arc animation
    lv_coord_t arc_start = count > 0 ? (1 - cosf(count / 180.0f * PI)) * 270 : 0;
    lv_coord_t arc_len = (sinf(count / 180.0f * PI) + 1) * 135;

    for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
        lv_arc_set_bg_angles(arc[i], arc_start, arc_len);
        lv_arc_set_rotation(arc[i], (count + 120 * (i + 1)) % 360);
        lv_obj_set_style_arc_opa(arc[i], count + 40*(i+1), 0);
    }

    count += 5;
    if (count == 90)
      count = -90;

    timer_ctx->count_val = count;
    timer_ctx->flip = flip;
}


lv_obj_t* ui_splash_create(void)
{
  lv_obj_t* scr = lv_obj_create(NULL);

  // Create arcs
  for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
      arc[i] = lv_arc_create(scr);

      // Set arc caption
      lv_obj_set_size(arc[i], 75 - 15 * i, 75 - 15 * i);
      lv_arc_set_bg_angles(arc[i], 120 * i, 10 + 120 * i);
      lv_arc_set_value(arc[i], 0);

      // Set arc style
      lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
      lv_obj_set_style_arc_width(arc[i], 7, 0);
      lv_obj_set_style_arc_color(arc[i], arc_color[i], 0);

      // Make arc center
      //lv_obj_center(arc[i]);
      lv_obj_align(arc[i], LV_ALIGN_CENTER, -40, 0);
  }

  label1 = lv_label_create(scr);
  lv_obj_set_width(label1, 156);
  lv_label_set_text(label1, "MakeIt Labs");
  lv_obj_align(label1, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_text_font(label1, &lv_font_montserrat_14, 0);

  label2 = lv_label_create(scr);
  lv_obj_set_width(label2, 156);
  lv_label_set_text(label2, "uRATT");
  lv_obj_align(label2, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(label2, &lv_font_montserrat_20, 0);

  // Create image
  img_logo = lv_img_create(scr);
  lv_img_set_src(img_logo, &ratt_head);
  lv_obj_align(img_logo, LV_ALIGN_CENTER, 40, 0);
  lv_obj_set_style_img_opa(img_logo, 128, 0);

  // Create timer for animation
  static my_timer_context_t my_tim_ctx = {
      .count_val = -90,
      .flip = true
  };
  my_tim_ctx.scr = scr;
  timer = lv_timer_create(anim_timer_cb, 100, &my_tim_ctx);
  return scr;
}
