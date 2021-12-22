#include <stdio.h>
#include "ui_wrapper.h"
#include "ui/ui_splash.h"
#include "ui/ui_idle.h"
#include "ui/ui_access.h"
#include "ui/ui_sleep.h"

typedef struct {
    lv_obj_t *scr;
    int count_val;
    int status_count;
} ui_timer_context_t;

static ui_timer_context_t progress_ctx = {
    .count_val = 0,
    .status_count = 0,
    .scr = 0
};

static lv_timer_t* ui_timer;
static lv_obj_t* ui_splash;
static lv_obj_t* ui_idle;
static lv_obj_t* ui_access;
static lv_obj_t* ui_sleep;

void progress_cb(lv_timer_t * timer)
{
  ui_timer_context_t *timer_ctx = (ui_timer_context_t *) timer->user_data;
  int count = timer_ctx->count_val;

  ui_idle_set_acl_download_progress(count);

  printf("progress %d\n", count);
  count++;
  if (count > 100)
    lv_timer_del(timer);

  timer_ctx->count_val = count;
}

void ui_timer_cb(lv_timer_t * timer)
{
    ui_timer_context_t *timer_ctx = (ui_timer_context_t *) timer->user_data;
    int count = timer_ctx->count_val;
    int status_count = timer_ctx->status_count;
    //lv_obj_t *scr = timer_ctx->scr;

    if (count == 0) {
        lv_scr_load(ui_splash);
    } else if (count == 3) {
        lv_scr_load(ui_idle);
        //short circuit
        //count = 250 - 1;
    } else if (count == 5) {
        count = 100 - 1;
        status_count = 0;
    } else if (count >= 100 && count < 200 && (count % 5) == 0) {
        if (status_count < WIFI_STATUS_MAX) {
            ui_idle_set_wifi_status(status_count);
            printf("tick %d: wifi status=%d\n", count, status_count);
            status_count++;
        } else {
            status_count = 0;
            count = 200 - 1;
        }
    } else if (count >= 200 && count < 205) {
        const int rssi_table[] = { -90, -80, -70, -60, -50 };
        int rssi = rssi_table[status_count];
        ui_idle_set_rssi(rssi);
        printf("tick %d: wifi rssi=%d\n", count, rssi);
        status_count++;
    } else if (count > 205 && count < 250) {
        status_count = 0;
        count = 250 - 1;

    } else if (count == 250) {
        ui_idle_set_acl_status(ACL_STATUS_DOWNLOADING);
        progress_ctx.count_val = 0;
        lv_timer_create(progress_cb, 100, &progress_ctx);

    } else if (count == 260) {
      count = 300 - 1;
    } else if (count >= 300 && count < 400 && (count % 5) == 0) {
        if (status_count < ACL_STATUS_MAX) {
            ui_idle_set_acl_status(status_count);
            printf("tick %d: ACL status=%d\n", count, status_count);
            status_count++;
        } else {
            status_count = 0;
            count = 400 - 1;
        }
    } else if (count >= 400 && count < 500 && (count % 5) == 0) {
        if (status_count < MQTT_STATUS_MAX) {
            ui_idle_set_mqtt_status(status_count);
            printf("tick %d: MQTT status=%d\n", count, status_count);
            status_count++;
        } else {
            status_count = 0;
            count = 500 - 1;
        }
    } else if (count == 500) {
        lv_scr_load(ui_access);
        ui_access_set_user("Allowed.Person", true);
        printf("tick %d: Allowed Access\n", count);
    } else if (count == 508) {
        lv_scr_load(ui_idle);
    } else if (count == 510) {
        lv_scr_load(ui_access);
        ui_access_set_user("Denied.Person", false);
        printf("tick %d: Denied Access\n", count);
    } else if (count == 518) {
        lv_scr_load(ui_idle);
    } else if (count == 520) {
        lv_scr_load(ui_sleep);
    } else if (count > 520 && count < 530) {
        ui_sleep_set_countdown(10 - (count - 520));
    }

    count++;
    timer_ctx->count_val = count;
    timer_ctx->status_count = status_count;
}
void ui_wrapper(void)
{
    ui_splash = ui_splash_create();
    ui_idle = ui_idle_create();
    ui_access = ui_access_create();
    ui_sleep = ui_sleep_create();

    static ui_timer_context_t ui_tim_ctx = {
        .count_val = 0,
        .status_count = 0
    };
    ui_tim_ctx.scr = lv_scr_act();
    ui_timer = lv_timer_create(ui_timer_cb, 1000, &ui_tim_ctx);
}
