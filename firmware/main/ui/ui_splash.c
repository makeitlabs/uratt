#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

LV_IMG_DECLARE(makeit_logo)
LV_IMG_DECLARE(ratt_logo)

static lv_obj_t *img_logo;


typedef struct {
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

static my_timer_context_t s_tim_ctx = {
    .count_val = 0,
};


static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *) timer->user_data;
    int count = timer_ctx->count_val;

    if (++count == 10) {
      lv_img_set_src(img_logo, &ratt_logo);
      lv_timer_pause(timer);
    }

    timer_ctx->count_val = count;
}


static lv_timer_t *timer;

void ui_splash_reset()
{
  lv_timer_pause(timer);
  s_tim_ctx.count_val = 0;
  lv_img_set_src(img_logo, &makeit_logo);
  lv_timer_resume(timer);
}

lv_obj_t* ui_splash_create(void)
{
  lv_obj_t* scr = lv_obj_create(NULL);

  // Create image
  img_logo = lv_img_create(scr);
  lv_img_set_src(img_logo, &makeit_logo);
  lv_obj_align(img_logo, LV_ALIGN_CENTER, 0, 0);

  // Create timer for animation
  s_tim_ctx.scr = scr;
  timer = lv_timer_create(anim_timer_cb, 100, &s_tim_ctx);

  return scr;
}
