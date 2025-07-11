#include "hmi_main.h"

static const char *TAG = "SCREEN_DATA";

// Data screen objects
static lv_obj_t *screen_data = NULL;

void screen_data_create(void)
{
    if (screen_data != NULL) {
        return; // Already created
    }

    screen_data = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_data, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(screen_data);
    lv_label_set_text(title, "Data & Trends");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Data displays
    create_value_display(screen_data, "Learning Gain pH:", "1.0", "", 50, 80);
    create_value_display(screen_data, "Learning Gain ORP:", "1.0", "", 300, 80);
    create_value_display(screen_data, "Base Acid Rate:", "10.0", "ml/hr", 550, 80);
    create_value_display(screen_data, "Runtime Hours:", "0.0", "hrs", 50, 150);

    ESP_LOGI(TAG, "Data screen created");
}

void screen_data_update(void)
{
    if (screen_data == NULL) {
        return;
    }
    // Update data displays based on g_system_data
}
