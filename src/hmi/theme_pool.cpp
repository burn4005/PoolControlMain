#include "theme_pool.h"

static lv_style_t s_style_screen;
static lv_style_t s_style_card;
static lv_style_t s_style_btn;
static bool s_theme_ready;

void theme_pool_init(lv_disp_t *disp)
{
    if (!s_theme_ready) {
        lv_style_init(&s_style_screen);
        lv_style_set_bg_color(&s_style_screen, lv_color_hex(THEME_BG));
        lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);

        lv_style_init(&s_style_card);
        lv_style_set_bg_color(&s_style_card, lv_color_hex(THEME_CARD));
        lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
        lv_style_set_border_color(&s_style_card, lv_color_hex(0x22335A));
        lv_style_set_border_width(&s_style_card, 2);
        lv_style_set_radius(&s_style_card, 16);
        lv_style_set_pad_all(&s_style_card, 12);

        lv_style_init(&s_style_btn);
        lv_style_set_radius(&s_style_btn, 14);
        lv_style_set_min_width(&s_style_btn, 56);
        lv_style_set_min_height(&s_style_btn, 56);
        lv_style_set_text_color(&s_style_btn, lv_color_hex(THEME_TEXT_PRIMARY));

        s_theme_ready = true;
    }

    lv_theme_t *theme = lv_theme_default_init(disp, lv_color_hex(THEME_PRIMARY), lv_color_hex(THEME_DANGER), true, &lv_font_montserrat_14);
    lv_disp_set_theme(disp, theme);
}

void theme_style_screen(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_screen, 0);
}

void theme_style_card(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_card, 0);
}

void theme_style_button(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_add_style(obj, &s_style_btn, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}
