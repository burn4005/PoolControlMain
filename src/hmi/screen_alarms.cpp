#include "hmi_main.h"

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_list;
static lv_obj_t *s_history;

static void ack_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char *alarm = (const char *)lv_event_get_user_data(e);
        send_command_string("ack_alarm", "id", alarm);
    }
}

static void add_alarm_row(const char *id, const char *text, bool active)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_size(row, 730, 58);
    lv_obj_set_style_bg_color(row, active ? lv_color_hex(0x4A1C29) : lv_color_hex(0x2B3A1F), 0);
    lv_obj_set_style_border_width(row, 0, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_fmt(lbl, "%s: %s", id, text);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *ack = create_button_with_label(row, "ACK", 640, 6, 80, 44);
    lv_obj_add_event_cb(ack, ack_btn_cb, LV_EVENT_CLICKED, (void *)id);
}

void screen_alarms_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Alarms");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *active_card = create_status_card(s_screen, "Current Alarms", 16, 74, 766, 194);
    s_list = lv_obj_create(active_card);
    lv_obj_set_size(s_list, 740, 148);
    lv_obj_set_pos(s_list, 8, 36);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, 0);

    lv_obj_t *history_card = create_status_card(s_screen, "History", 16, 278, 766, 126);
    s_history = lv_label_create(history_card);
    lv_label_set_text(s_history, "No historical events");
    lv_obj_set_pos(s_history, 10, 42);

    ui_create_nav_bar(s_screen, SCREEN_ALARMS);
}

void screen_alarms_update(void)
{
    if (s_screen == NULL) {
        return;
    }

    lv_obj_clean(s_list);
    add_alarm_row("PUMP", g_system_data_view.pump_alarm_active ? "Active" : "Cleared", g_system_data_view.pump_alarm_active);
    add_alarm_row("CHLOR", g_system_data_view.chlorinator_alarm_active ? "Active" : "Cleared", g_system_data_view.chlorinator_alarm_active);
    add_alarm_row("ACID", g_system_data_view.acid_low_alarm_active ? "Low level" : "Normal", g_system_data_view.acid_low_alarm_active);

    lv_label_set_text_fmt(s_history, "Sensors: %s", g_system_data_view.sensors_healthy ? "Healthy" : "Fault");
}

lv_obj_t *get_alarms_screen(void)
{
    return s_screen;
}
