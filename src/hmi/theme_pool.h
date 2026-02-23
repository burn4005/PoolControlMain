#ifndef THEME_POOL_H
#define THEME_POOL_H

#include "lvgl.h"

#define THEME_BG 0x1A1A2E
#define THEME_CARD 0x16213E
#define THEME_PRIMARY 0x0F9BF2
#define THEME_SUCCESS 0x00E676
#define THEME_WARNING 0xFFD600
#define THEME_DANGER 0xFF1744
#define THEME_TEXT_PRIMARY 0xE0E0E0
#define THEME_TEXT_SECONDARY 0x9E9E9E

void theme_pool_init(lv_disp_t *disp);
void theme_style_screen(lv_obj_t *obj);
void theme_style_card(lv_obj_t *obj);
void theme_style_button(lv_obj_t *obj, lv_color_t color);

#endif
