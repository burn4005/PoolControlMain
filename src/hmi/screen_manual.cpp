#include "hmi_main.h"

static const char *TAG = "SCREEN_MANUAL";

// Manual control screen objects
static lv_obj_t *screen_manual = NULL;
static lv_obj_t *pump_btn = NULL;
static lv_obj_t *chlorinator_btn = NULL;
static lv_obj_t *light_btn = NULL;
static lv_obj_t *dose_btn = NULL;

// Button event handlers
static void pump_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Pump button clicked");
        // Toggle pump relay
        send_command_bool("set_pump_relay", "", !g_system_data.pump_relay_on);
    }
}

static void chlorinator_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Chlorinator button clicked");
        // Toggle chlorinator relay
        send_command_bool("set_chlorinator_relay", "", !g_system_data.chlorinator_relay_on);
    }
}

static void light_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Light button clicked");
        // Toggle light relay
        send_command_bool("set_light_relay", "", !g_system_data.light_relay_on);
    }
}

static void dose_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Manual dose button clicked");
        // Manual acid dose - 10ml
        send_command("manual_dose", "", 10.0f);
    }
}

void screen_manual_create(void)
{
    if (screen_manual != NULL) {
        return; // Already created
    }

    screen_manual = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_manual, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(screen_manual);
    lv_label_set_text(title, "Manual Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Pump control button
    pump_btn = create_button_with_label(screen_manual, "PUMP", 50, 80, 150, 60);
    lv_obj_add_event_cb(pump_btn, pump_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Chlorinator control button
    chlorinator_btn = create_button_with_label(screen_manual, "CHLORINATOR", 220, 80, 150, 60);
    lv_obj_add_event_cb(chlorinator_btn, chlorinator_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Light control button
    light_btn = create_button_with_label(screen_manual, "LIGHTS", 390, 80, 150, 60);
    lv_obj_add_event_cb(light_btn, light_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Manual dose button
    dose_btn = create_button_with_label(screen_manual, "DOSE 10ml", 560, 80, 150, 60);
    lv_obj_add_event_cb(dose_btn, dose_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Status displays
    create_value_display(screen_manual, "Pump Status:", get_pump_status_string(g_system_data.pump_status), "", 50, 180);
    create_value_display(screen_manual, "Pump Current:", "0.0", "A", 250, 180);
    create_value_display(screen_manual, "Chlorinator Current:", "0.0", "A", 450, 180);

    ESP_LOGI(TAG, "Manual control screen created");
}

void screen_manual_update(void)
{
    if (screen_manual == NULL) {
        return;
    }

    // Update button colors based on relay states
    if (pump_btn) {
        lv_color_t color = g_system_data.pump_relay_on ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
        lv_obj_set_style_bg_color(pump_btn, color, 0);
    }

    if (chlorinator_btn) {
        lv_color_t color = g_system_data.chlorinator_relay_on ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
        lv_obj_set_style_bg_color(chlorinator_btn, color, 0);
    }

    if (light_btn) {
        lv_color_t color = g_system_data.light_relay_on ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
        lv_obj_set_style_bg_color(light_btn, color, 0);
    }
}
