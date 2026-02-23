#include "hmi_main.h"

#include <math.h>

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_ph_slider;
static lv_obj_t *s_orp_slider;
static lv_obj_t *s_duty_slider;
static lv_obj_t *s_info;

static void apply_confirm_cb(bool confirmed, void *ctx)
{
    (void)ctx;
    if (!confirmed) {
        return;
    }

    float ph = (float)lv_slider_get_value(s_ph_slider) / 10.0f;
    float orp = (float)lv_slider_get_value(s_orp_slider);
    float duty = (float)lv_slider_get_value(s_duty_slider);

    send_command("set_ph_target", "", ph);
    send_command("set_orp_target", "", orp);
    send_command("set_chlorinator_duty", "", duty);
}

static void apply_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_create_confirm_dialog("Apply Settings", "Send target changes to controller?", apply_confirm_cb, NULL);
    }
}

void screen_settings_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *card = create_status_card(s_screen, "Targets", 16, 76, 766, 272);

    lv_obj_t *ph_lbl = lv_label_create(card);
    lv_label_set_text(ph_lbl, "pH target");
    lv_obj_set_pos(ph_lbl, 10, 42);
    s_ph_slider = lv_slider_create(card);
    lv_obj_set_size(s_ph_slider, 580, 18);
    lv_obj_set_pos(s_ph_slider, 140, 46);
    lv_slider_set_range(s_ph_slider, 70, 78);

    lv_obj_t *orp_lbl = lv_label_create(card);
    lv_label_set_text(orp_lbl, "ORP target");
    lv_obj_set_pos(orp_lbl, 10, 110);
    s_orp_slider = lv_slider_create(card);
    lv_obj_set_size(s_orp_slider, 580, 18);
    lv_obj_set_pos(s_orp_slider, 140, 114);
    lv_slider_set_range(s_orp_slider, 600, 800);

    lv_obj_t *duty_lbl = lv_label_create(card);
    lv_label_set_text(duty_lbl, "Duty cycle");
    lv_obj_set_pos(duty_lbl, 10, 178);
    s_duty_slider = lv_slider_create(card);
    lv_obj_set_size(s_duty_slider, 580, 18);
    lv_obj_set_pos(s_duty_slider, 140, 182);
    lv_slider_set_range(s_duty_slider, 0, 100);

    lv_obj_t *apply_btn = create_button_with_label(card, "Apply", 620, 210, 120, 48);
    lv_obj_add_event_cb(apply_btn, apply_btn_cb, LV_EVENT_CLICKED, NULL);

    s_info = lv_label_create(card);
    lv_obj_set_style_text_color(s_info, lv_color_hex(THEME_TEXT_SECONDARY), 0);
    lv_obj_set_pos(s_info, 140, 222);

    ui_create_nav_bar(s_screen, SCREEN_SETTINGS);
}

void screen_settings_update(void)
{
    if (s_screen == NULL) {
        return;
    }

    lv_slider_set_value(s_ph_slider, (int32_t)lroundf(g_system_data_view.ph_target * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(s_orp_slider, (int32_t)lroundf(g_system_data_view.orp_target), LV_ANIM_OFF);
    lv_slider_set_value(s_duty_slider, (int32_t)lroundf(g_system_data_view.chlorinator_duty_cycle), LV_ANIM_OFF);

    char info[128];
    snprintf(info, sizeof(info), "Volume %.0f L    HCl %.1f%%", g_system_data_view.pool_volume_liters, g_system_data_view.hcl_concentration_percent);
    lv_label_set_text(s_info, info);
}

lv_obj_t *get_settings_screen(void)
{
    return s_screen;
}
