#include "hmi_main.h"

#include "theme_pool.h"
#include "trend_data.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_alarm_banner;
static lv_obj_t *s_temp_value;
static lv_obj_t *s_ph_value;
static lv_obj_t *s_orp_value;
static lv_obj_t *s_equipment_summary;
static lv_obj_t *s_spark_chart;

static lv_obj_t *create_gauge(lv_obj_t *parent, const char *title, lv_coord_t x)
{
    lv_obj_t *card = create_status_card(parent, title, x, 62, 248, 190);
    lv_obj_t *arc = lv_arc_create(card);
    lv_obj_set_size(arc, 110, 110);
    lv_obj_align(arc, LV_ALIGN_LEFT_MID, 4, 14);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_rotation(arc, 180);
    lv_arc_set_range(arc, 0, 1000);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, -10, 10);
    return value;
}

void screen_dashboard_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Pool Dashboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    s_alarm_banner = lv_obj_create(s_screen);
    lv_obj_set_size(s_alarm_banner, 360, 44);
    lv_obj_align(s_alarm_banner, LV_ALIGN_TOP_RIGHT, -16, 18);
    lv_obj_set_style_bg_color(s_alarm_banner, lv_color_hex(THEME_DANGER), 0);
    lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *alarm_text = lv_label_create(s_alarm_banner);
    lv_label_set_text(alarm_text, "ACTIVE ALARM");
    lv_obj_center(alarm_text);

    s_ph_value = create_gauge(s_screen, "pH", 16);
    s_orp_value = create_gauge(s_screen, "ORP", 276);
    s_temp_value = create_gauge(s_screen, "Temperature", 536);

    lv_obj_t *equip_card = create_status_card(s_screen, "Equipment", 16, 266, 382, 138);
    s_equipment_summary = lv_label_create(equip_card);
    lv_label_set_text(s_equipment_summary, "Pump --  Chlorinator --  Lights --");
    lv_obj_set_style_text_color(s_equipment_summary, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(s_equipment_summary, &lv_font_montserrat_20, 0);
    lv_obj_align(s_equipment_summary, LV_ALIGN_CENTER, 0, 14);

    lv_obj_t *spark_wrap = create_status_card(s_screen, "Trend Preview", 416, 266, 368, 138);
    s_spark_chart = ui_create_trend_chart(spark_wrap, 8, 36, 350, 94);

    ui_create_nav_bar(s_screen, SCREEN_DASHBOARD);
}

void screen_dashboard_update(void)
{
    if (s_screen == NULL) {
        return;
    }

    char text[32];

    snprintf(text, sizeof(text), "%.2f", g_system_data_view.ph);
    lv_label_set_text(s_ph_value, text);
    snprintf(text, sizeof(text), "%.0f", g_system_data_view.orp);
    lv_label_set_text(s_orp_value, text);
    snprintf(text, sizeof(text), "%.1f C", g_system_data_view.temperature);
    lv_label_set_text(s_temp_value, text);

    snprintf(text, sizeof(text), "P:%s C:%s L:%s",
             g_system_data_view.pump_relay_on ? "ON" : "OFF",
             g_system_data_view.chlorinator_relay_on ? "ON" : "OFF",
             g_system_data_view.light_relay_on ? "ON" : "OFF");
    lv_label_set_text(s_equipment_summary, text);

    const bool has_alarm = g_system_data_view.pump_alarm_active || g_system_data_view.chlorinator_alarm_active || g_system_data_view.acid_low_alarm_active;
    if (has_alarm) {
        lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    }

    ui_chart_load_metric(s_spark_chart, TREND_METRIC_PH, TREND_RANGE_24H);
}

lv_obj_t *get_dashboard_screen(void)
{
    return s_screen;
}
