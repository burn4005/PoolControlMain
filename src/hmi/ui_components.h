#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include "hmi_main.h"
#include "trend_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_confirm_cb_t)(bool confirmed, void *ctx);

lv_obj_t *ui_create_nav_bar(lv_obj_t *parent, screen_id_t active_screen);
lv_obj_t *ui_create_toggle_card(lv_obj_t *parent, const char *title, const char *state_text, bool on, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_create_trend_chart(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
void ui_chart_load_metric(lv_obj_t *chart, trend_metric_t metric, trend_range_t range);
void ui_create_confirm_dialog(const char *title, const char *message, ui_confirm_cb_t cb, void *ctx);
void ui_attach_touch_feedback(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif
