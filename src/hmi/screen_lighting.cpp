#include "hmi_main.h"

#include "theme_pool.h"
#include "ui_components.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_preview;

static void mode_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    static const char *mode_names[] = {
        "off", "blue", "pink", "red", "yellow", "green", "cyan", "white", "mode1", "mode2", "mode3", "mode4", "brightness"
    };

    send_command_string("set_light_mode", "mode", mode_names[mode]);
}

void screen_lighting_create(void)
{
    if (s_screen != NULL) {
        return;
    }

    s_screen = lv_obj_create(NULL);
    theme_style_screen(s_screen);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Lighting");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 12);

    lv_obj_t *card = create_status_card(s_screen, "Mode", 16, 74, 350, 138);
    s_mode_label = lv_label_create(card);
    lv_label_set_text(s_mode_label, "Current: OFF");
    lv_obj_set_style_text_font(s_mode_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_mode_label, LV_ALIGN_LEFT_MID, 10, 8);

    s_preview = lv_obj_create(card);
    lv_obj_set_size(s_preview, 64, 64);
    lv_obj_set_style_radius(s_preview, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_preview, lv_color_hex(0x666666), 0);
    lv_obj_align(s_preview, LV_ALIGN_RIGHT_MID, -12, 10);

    lv_obj_t *grid = create_status_card(s_screen, "Colors", 382, 74, 400, 290);
    const lv_color_t colors[] = {
        lv_color_hex(0x666666), lv_color_hex(0x007BFF), lv_color_hex(0xFF69B4), lv_color_hex(0xF44336), lv_color_hex(0xFFD600),
        lv_color_hex(0x00E676), lv_color_hex(0x00BCD4), lv_color_hex(0xECEFF1)
    };

    for (int i = 0; i < 8; i++) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 82, 82);
        lv_obj_set_pos(btn, 10 + (i % 4) * 94, 40 + (i / 4) * 96);
        lv_obj_set_style_bg_color(btn, colors[i], 0);
        lv_obj_set_style_radius(btn, 16, 0);
        ui_attach_touch_feedback(btn);
        lv_obj_add_event_cb(btn, mode_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    ui_create_nav_bar(s_screen, SCREEN_LIGHTING);
}

void screen_lighting_update(void)
{
    if (s_screen == NULL) {
        return;
    }

    int mode = g_system_data_view.light_relay_on ? g_system_data_view.current_light_mode : 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "Current: %s", get_light_mode_string(mode));
    lv_label_set_text(s_mode_label, buf);

    lv_color_t preview_color = lv_color_hex(0x666666);
    switch (mode) {
        case 1: preview_color = lv_color_hex(0x007BFF); break;
        case 2: preview_color = lv_color_hex(0xFF69B4); break;
        case 3: preview_color = lv_color_hex(0xF44336); break;
        case 4: preview_color = lv_color_hex(0xFFD600); break;
        case 5: preview_color = lv_color_hex(0x00E676); break;
        case 6: preview_color = lv_color_hex(0x00BCD4); break;
        case 7: preview_color = lv_color_hex(0xECEFF1); break;
        default: break;
    }
    lv_obj_set_style_bg_color(s_preview, preview_color, 0);
}

lv_obj_t *get_lighting_screen(void)
{
    return s_screen;
}
