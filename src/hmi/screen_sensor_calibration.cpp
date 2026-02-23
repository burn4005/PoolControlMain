#include "hmi_main.h"

#include <stdlib.h>

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_ph;
static lv_obj_t *s_orp;
static lv_obj_t *s_status;

static void cal_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    if (code == 0) {
        send_command("calibrate_ph", "buffer_value", (float)atof(lv_textarea_get_text(s_ph)));
    } else {
        send_command("calibrate_orp", "standard_mv", (float)atof(lv_textarea_get_text(s_orp)));
    }
}

void screen_sensor_calibration_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Sensor Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *card = create_status_card(s_screen, "Wizard", 16, 74, 766, 320);

    lv_obj_t *l1 = lv_label_create(card);
    lv_label_set_text(l1, "pH Buffer");
    lv_obj_set_pos(l1, 16, 44);
    s_ph = lv_textarea_create(card);
    lv_obj_set_size(s_ph, 180, 50);
    lv_obj_set_pos(s_ph, 130, 34);
    lv_textarea_set_text(s_ph, "7.0");

    lv_obj_t *l2 = lv_label_create(card);
    lv_label_set_text(l2, "ORP Std (mV)");
    lv_obj_set_pos(l2, 16, 124);
    s_orp = lv_textarea_create(card);
    lv_obj_set_size(s_orp, 180, 50);
    lv_obj_set_pos(s_orp, 130, 114);
    lv_textarea_set_text(s_orp, "470");

    lv_obj_t *b1 = create_button_with_label(card, "Calibrate pH", 340, 34, 170, 56);
    lv_obj_t *b2 = create_button_with_label(card, "Calibrate ORP", 340, 114, 170, 56);
    lv_obj_add_event_cb(b1, cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_add_event_cb(b2, cal_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    s_status = lv_label_create(card);
    lv_label_set_text(s_status, "Ready");
    lv_obj_set_pos(s_status, 16, 240);

    ui_create_nav_bar(s_screen, SCREEN_SENSOR_CALIBRATION);
}

void screen_sensor_calibration_update(void)
{
    if (s_status == NULL) {
        return;
    }
    lv_label_set_text_fmt(s_status, "Current pH %.2f, ORP %.0f mV", g_system_data_view.ph, g_system_data_view.orp);
}

lv_obj_t *get_sensor_calibration_screen(void)
{
    return s_screen;
}
