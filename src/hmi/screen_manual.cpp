#include "hmi_main.h"

#include <stdlib.h>

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_status;

typedef enum {
    ACTION_TOGGLE_PUMP,
    ACTION_TOGGLE_CHLOR,
    ACTION_TOGGLE_LIGHT,
    ACTION_DOSE_5,
    ACTION_DOSE_10,
    ACTION_DOSE_25,
} manual_action_t;

static void manual_confirm_cb(bool confirmed, void *ctx)
{
    if (!confirmed) {
        return;
    }

    manual_action_t action = (manual_action_t)(intptr_t)ctx;
    switch (action) {
        case ACTION_TOGGLE_PUMP: send_command_bool("set_pump_relay", "", !g_system_data_view.pump_relay_on); break;
        case ACTION_TOGGLE_CHLOR: send_command_bool("set_chlorinator_relay", "", !g_system_data_view.chlorinator_relay_on); break;
        case ACTION_TOGGLE_LIGHT: send_command_bool("set_light_relay", "", !g_system_data_view.light_relay_on); break;
        case ACTION_DOSE_5: send_command("manual_dose", "ml", 5.0f); break;
        case ACTION_DOSE_10: send_command("manual_dose", "ml", 10.0f); break;
        case ACTION_DOSE_25: send_command("manual_dose", "ml", 25.0f); break;
    }
}

static void action_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    manual_action_t action = (manual_action_t)(intptr_t)lv_event_get_user_data(e);
    ui_create_confirm_dialog("Confirm Action", "Execute manual command?", manual_confirm_cb, (void *)(intptr_t)action);
}

void screen_manual_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Manual Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *pump = ui_create_toggle_card(s_screen, "Pump", "Toggle", g_system_data_view.pump_relay_on, 16, 72, 246, 138);
    lv_obj_t *chlor = ui_create_toggle_card(s_screen, "Chlorinator", "Toggle", g_system_data_view.chlorinator_relay_on, 276, 72, 246, 138);
    lv_obj_t *light = ui_create_toggle_card(s_screen, "Lights", "Toggle", g_system_data_view.light_relay_on, 536, 72, 246, 138);

    lv_obj_add_event_cb(pump, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_TOGGLE_PUMP);
    lv_obj_add_event_cb(chlor, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_TOGGLE_CHLOR);
    lv_obj_add_event_cb(light, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_TOGGLE_LIGHT);

    lv_obj_t *dose_card = create_status_card(s_screen, "Acid Dose Presets", 16, 230, 766, 136);
    lv_obj_t *b1 = create_button_with_label(dose_card, "5 ml", 16, 42, 130, 72);
    lv_obj_t *b2 = create_button_with_label(dose_card, "10 ml", 164, 42, 130, 72);
    lv_obj_t *b3 = create_button_with_label(dose_card, "25 ml", 312, 42, 130, 72);
    lv_obj_add_event_cb(b1, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_DOSE_5);
    lv_obj_add_event_cb(b2, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_DOSE_10);
    lv_obj_add_event_cb(b3, action_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACTION_DOSE_25);

    s_status = lv_label_create(dose_card);
    lv_label_set_text(s_status, "Pump speed: OFF / LOW / MED / HIGH");
    lv_obj_align(s_status, LV_ALIGN_RIGHT_MID, -18, 0);

    ui_create_nav_bar(s_screen, SCREEN_MANUAL);
}

void screen_manual_update(void)
{
    if (s_status == NULL) {
        return;
    }
    char status[64];
    snprintf(status, sizeof(status), "Pump speed: %s", get_pump_status_string(g_system_data_view.pump_status));
    lv_label_set_text(s_status, status);
}

lv_obj_t *get_manual_screen(void)
{
    return s_screen;
}
