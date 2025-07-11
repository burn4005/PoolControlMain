#include "hmi_main.h"

static const char *TAG = "SCREEN_SETTINGS";

// Settings screen objects
static lv_obj_t *screen_settings = NULL;

void screen_settings_create(void)
{
    if (screen_settings != NULL) {
        return; // Already created
    }

    screen_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_settings, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(screen_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Settings placeholder
    lv_obj_t *placeholder = lv_label_create(screen_settings);
    lv_label_set_text(placeholder, "Settings screen - Implementation pending");
    lv_obj_set_style_text_color(placeholder, lv_color_white(), 0);
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "Settings screen created");
}

void screen_settings_update(void)
{
    if (screen_settings == NULL) {
        return;
    }
    // Update settings display
}
