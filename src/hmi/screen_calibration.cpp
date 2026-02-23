#include "hmi_main.h"

#include <stdlib.h>

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_target;
static lv_obj_t *s_actual;
static lv_obj_t *s_status;

static void start_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    float target = (float)atof(lv_textarea_get_text(s_target));
    send_command("pump_cal_start", "target_volume", target);
    lv_label_set_text(s_status, "Calibration started");
}

static void complete_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    float actual = (float)atof(lv_textarea_get_text(s_actual));
    send_command("pump_cal_complete", "actual_volume", actual);
    lv_label_set_text(s_status, "Calibration completed");
}

void screen_calibration_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Pump Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *card = create_status_card(s_screen, "Wizard", 16, 74, 766, 320);

    lv_obj_t *l1 = lv_label_create(card);
    lv_label_set_text(l1, "Target (ml)");
    lv_obj_set_pos(l1, 16, 50);
    s_target = lv_textarea_create(card);
    lv_obj_set_size(s_target, 180, 50);
    lv_obj_set_pos(s_target, 130, 40);
    lv_textarea_set_one_line(s_target, true);
    lv_textarea_set_text(s_target, "100");

    lv_obj_t *l2 = lv_label_create(card);
    lv_label_set_text(l2, "Actual (ml)");
    lv_obj_set_pos(l2, 16, 130);
    s_actual = lv_textarea_create(card);
    lv_obj_set_size(s_actual, 180, 50);
    lv_obj_set_pos(s_actual, 130, 120);
    lv_textarea_set_one_line(s_actual, true);

    lv_obj_t *start = create_button_with_label(card, "Start", 340, 40, 160, 56);
    lv_obj_t *done = create_button_with_label(card, "Complete", 340, 120, 160, 56);
    lv_obj_add_event_cb(start, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(done, complete_btn_cb, LV_EVENT_CLICKED, NULL);

    s_status = lv_label_create(card);
    lv_label_set_text(s_status, "Ready");
    lv_obj_set_pos(s_status, 16, 240);

    ui_create_nav_bar(s_screen, SCREEN_CALIBRATION);
}

void screen_calibration_update(void)
{
    if (s_status == NULL) {
        return;
    }
    lv_label_set_text_fmt(s_status, "Pump %s, current %.2f A", get_pump_status_string(g_system_data_view.pump_status), g_system_data_view.pump_current);
}

lv_obj_t *get_calibration_screen(void)
{
    return s_screen;
}
