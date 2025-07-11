#include "hmi_main.h"

static const char *TAG = "SCREEN_ALARMS";

// Alarms screen objects
static lv_obj_t *screen_alarms = NULL;

void screen_alarms_create(void)
{
    if (screen_alarms != NULL) {
        return; // Already created
    }

    screen_alarms = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_alarms, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(screen_alarms);
    lv_label_set_text(title, "Alarms");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Alarm status displays
    create_value_display(screen_alarms, "Pump Alarm:", "OK", "", 50, 80);
    create_value_display(screen_alarms, "Chlorinator Alarm:", "OK", "", 300, 80);
    create_value_display(screen_alarms, "Acid Low Alarm:", "OK", "", 550, 80);
    create_value_display(screen_alarms, "Sensor Health:", "OK", "", 50, 150);

    ESP_LOGI(TAG, "Alarms screen created");
}

void screen_alarms_update(void)
{
    if (screen_alarms == NULL) {
        return;
    }
    // Update alarm status displays based on g_system_data
}
