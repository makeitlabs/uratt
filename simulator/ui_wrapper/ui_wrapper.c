#include "ui_wrapper.h"
#include "ui/ui_idle.h"

void ui_wrapper(void)
{
    lv_obj_t* ui = ui_idle_create();
    lv_scr_load(ui);

}
