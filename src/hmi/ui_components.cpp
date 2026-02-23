#include "ui_components.h"

#include <stdlib.h>
#include <string.h>

#include "theme_pool.h"

typedef struct {
    ui_confirm_cb_t cb;
    void *ctx;
} confirm_ctx_t;

static void nav_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen_id_t screen = (screen_id_t)(intptr_t)lv_event_get_user_data(e);
        gui_manager_switch_screen(screen);
    }
}

static void touch_fx_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_transform_zoom(obj, 244, 0);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_transform_zoom(obj, 256, 0);
    }
}

void ui_attach_touch_feedback(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, touch_fx_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, touch_fx_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, touch_fx_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

lv_obj_t *ui_create_nav_bar(lv_obj_t *parent, screen_id_t active_screen)
{
    static const struct {
        const char *label;
        screen_id_t id;
    } items[] = {
        {"Home", SCREEN_DASHBOARD},
        {"Manual", SCREEN_MANUAL},
        {"Lights", SCREEN_LIGHTING},
        {"Data", SCREEN_DATA},
        {"Alarms", SCREEN_ALARMS},
        {"Settings", SCREEN_SETTINGS},
    };

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LCD_H_RES - 20, 68);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -8);
    theme_style_card(bar);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t *btn = lv_btn_create(bar);
        ui_attach_touch_feedback(btn);
        theme_style_button(btn, items[i].id == active_screen ? lv_color_hex(THEME_PRIMARY) : lv_color_hex(0x24365D));
        lv_obj_set_size(btn, 120, 52);
        lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)items[i].id);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, items[i].label);
        lv_obj_center(lbl);
    }

    return bar;
}

lv_obj_t *ui_create_toggle_card(lv_obj_t *parent, const char *title, const char *state_text, bool on, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    theme_style_card(card);

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *state_lbl = lv_label_create(card);
    lv_label_set_text(state_lbl, state_text);
    lv_obj_set_style_text_color(state_lbl, on ? lv_color_hex(THEME_SUCCESS) : lv_color_hex(THEME_TEXT_SECONDARY), 0);
    lv_obj_align(state_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *led = lv_led_create(card);
    lv_obj_align(led, LV_ALIGN_RIGHT_MID, -8, 0);
    if (on) {
        lv_led_set_color(led, lv_color_hex(THEME_SUCCESS));
        lv_led_on(led);
    } else {
        lv_led_set_color(led, lv_color_hex(0x666666));
        lv_led_off(led);
    }

    return card;
}

lv_obj_t *ui_create_trend_chart(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, w, h);
    lv_obj_set_pos(wrap, x, y);
    theme_style_card(wrap);

    lv_obj_t *chart = lv_chart_create(wrap);
    lv_obj_set_size(chart, w - 24, h - 24);
    lv_obj_center(chart);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x111A30), 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 5, 6);
    lv_chart_set_point_count(chart, 180);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    lv_obj_set_style_line_color(chart, lv_color_hex(THEME_PRIMARY), LV_PART_ITEMS);

    return chart;
}

void ui_chart_load_metric(lv_obj_t *chart, trend_metric_t metric, trend_range_t range)
{
    static float temp[1600];
    const size_t count = trend_data_copy_series(metric, range, temp, sizeof(temp) / sizeof(temp[0]));

    lv_chart_series_t *series = lv_chart_get_series_next(chart, NULL);
    if (series == NULL) {
        series = lv_chart_add_series(chart, lv_color_hex(THEME_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);
    }

    int32_t ymin = INT32_MAX;
    int32_t ymax = INT32_MIN;
    size_t max_points = lv_chart_get_point_count(chart);
    if (max_points == 0) {
        return;
    }

    for (size_t i = 0; i < max_points; i++) {
        size_t src_idx = 0;
        if (count > 0) {
            src_idx = (i * count) / max_points;
            if (src_idx >= count) {
                src_idx = count - 1;
            }
        }

        int32_t v = (count > 0) ? (int32_t)temp[src_idx] : 0;
        series->y_points[i] = v;
        if (v < ymin) ymin = v;
        if (v > ymax) ymax = v;
    }

    if (ymin == INT32_MAX || ymax == INT32_MIN) {
        ymin = 0;
        ymax = 100;
    }

    if (ymax <= ymin) {
        ymax = ymin + 10;
    }

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, ymin - 5, ymax + 5);
    lv_chart_refresh(chart);
}

static void dialog_action_cb(lv_event_t *e)
{
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *msgbox = lv_event_get_current_target(e);
    const char *txt = lv_msgbox_get_active_btn_text(msgbox);
    if (ctx != NULL && ctx->cb != NULL) {
        ctx->cb(txt != NULL && strcmp(txt, "Yes") == 0, ctx->ctx);
    }
    free(ctx);
    lv_obj_del(msgbox);
}

void ui_create_confirm_dialog(const char *title, const char *message, ui_confirm_cb_t cb, void *ctx)
{
    static const char *btns[] = {"Yes", "No", ""};
    lv_obj_t *msgbox = lv_msgbox_create(NULL, title, message, btns, false);
    lv_obj_center(msgbox);

    confirm_ctx_t *cb_ctx = (confirm_ctx_t *)malloc(sizeof(confirm_ctx_t));
    if (cb_ctx != NULL) {
        cb_ctx->cb = cb;
        cb_ctx->ctx = ctx;
        lv_obj_add_event_cb(msgbox, dialog_action_cb, LV_EVENT_VALUE_CHANGED, cb_ctx);
    }
}
