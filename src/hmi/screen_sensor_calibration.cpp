#include "hmi_main.h"

static const char *TAG = "SCREEN_SENSOR_CAL";

// Sensor calibration screen objects
static lv_obj_t *sensor_cal_screen = NULL;
static lv_obj_t *current_ph_label = NULL;
static lv_obj_t *current_orp_label = NULL;
static lv_obj_t *ph_buffer_input = NULL;
static lv_obj_t *orp_standard_input = NULL;
static lv_obj_t *calibration_status_label = NULL;

// pH calibration buttons
static lv_obj_t *btn_ph_4 = NULL;
static lv_obj_t *btn_ph_7 = NULL;
static lv_obj_t *btn_ph_10 = NULL;
static lv_obj_t *btn_ph_custom = NULL;
static lv_obj_t *btn_clear_ph = NULL;

// ORP calibration buttons
static lv_obj_t *btn_orp_225 = NULL;
static lv_obj_t *btn_orp_470 = NULL;
static lv_obj_t *btn_orp_custom = NULL;
static lv_obj_t *btn_clear_orp = NULL;

// Navigation
static lv_obj_t *btn_back = NULL;

// Button event handlers
static void btn_ph_calibrate_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        float buffer_value = (float)(intptr_t)lv_event_get_user_data(e);
        
        // Send pH calibration command
        send_command("calibrate_ph", "buffer_value", buffer_value);
        
        char status_text[100];
        snprintf(status_text, sizeof(status_text), "pH calibration started with buffer %.1f", buffer_value);
        lv_label_set_text(calibration_status_label, status_text);
        lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
        
        ESP_LOGI(TAG, "pH calibration started with buffer %.1f", buffer_value);
    }
}

static void btn_ph_custom_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char* buffer_text = lv_textarea_get_text(ph_buffer_input);
        float buffer_value = atof(buffer_text);
        
        if (buffer_value >= 1.0 && buffer_value <= 14.0) {
            // Send pH calibration command
            send_command("calibrate_ph", "buffer_value", buffer_value);
            
            char status_text[100];
            snprintf(status_text, sizeof(status_text), "pH calibration started with buffer %.2f", buffer_value);
            lv_label_set_text(calibration_status_label, status_text);
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
            
            ESP_LOGI(TAG, "pH calibration started with custom buffer %.2f", buffer_value);
        } else {
            lv_label_set_text(calibration_status_label, "Invalid pH buffer value (1.0-14.0)");
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFF0000), 0);
        }
    }
}

static void btn_orp_calibrate_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        float standard_mv = (float)(intptr_t)lv_event_get_user_data(e);
        
        // Send ORP calibration command
        send_command("calibrate_orp", "standard_mv", standard_mv);
        
        char status_text[100];
        snprintf(status_text, sizeof(status_text), "ORP calibration started with %.0fmV standard", standard_mv);
        lv_label_set_text(calibration_status_label, status_text);
        lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
        
        ESP_LOGI(TAG, "ORP calibration started with %.0fmV standard", standard_mv);
    }
}

static void btn_orp_custom_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char* standard_text = lv_textarea_get_text(orp_standard_input);
        float standard_mv = atof(standard_text);
        
        if (standard_mv >= -2000 && standard_mv <= 2000) {
            // Send ORP calibration command
            send_command("calibrate_orp", "standard_mv", standard_mv);
            
            char status_text[100];
            snprintf(status_text, sizeof(status_text), "ORP calibration started with %.0fmV standard", standard_mv);
            lv_label_set_text(calibration_status_label, status_text);
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
            
            ESP_LOGI(TAG, "ORP calibration started with custom standard %.0fmV", standard_mv);
        } else {
            lv_label_set_text(calibration_status_label, "Invalid ORP standard (-2000 to 2000mV)");
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFF0000), 0);
        }
    }
}

static void btn_clear_ph_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Send clear pH calibration command
        send_command("clear_ph_cal", "", 0);
        
        lv_label_set_text(calibration_status_label, "pH calibration cleared");
        lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFFAA00), 0);
        
        ESP_LOGI(TAG, "pH calibration cleared");
    }
}

static void btn_clear_orp_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Send clear ORP calibration command
        send_command("clear_orp_cal", "", 0);
        
        lv_label_set_text(calibration_status_label, "ORP calibration cleared");
        lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFFAA00), 0);
        
        ESP_LOGI(TAG, "ORP calibration cleared");
    }
}

static void btn_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_DASHBOARD);
    }
}

void screen_sensor_calibration_create(void)
{
    ESP_LOGI(TAG, "Creating sensor calibration screen");
    
    sensor_cal_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sensor_cal_screen, lv_color_hex(0xE8F4FD), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(sensor_cal_screen);
    lv_label_set_text(title, "Sensor Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x003366), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current readings card
    lv_obj_t *readings_card = create_status_card(sensor_cal_screen, "Current Readings", 50, 50, 700, 80);
    
    current_ph_label = lv_label_create(readings_card);
    lv_label_set_text(current_ph_label, "pH: 7.20");
    lv_obj_set_style_text_font(current_ph_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(current_ph_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(current_ph_label, 50, 35);
    
    current_orp_label = lv_label_create(readings_card);
    lv_label_set_text(current_orp_label, "ORP: 685mV");
    lv_obj_set_style_text_font(current_orp_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(current_orp_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(current_orp_label, 400, 35);
    
    // pH Calibration Section
    lv_obj_t *ph_title = lv_label_create(sensor_cal_screen);
    lv_label_set_text(ph_title, "pH Calibration:");
    lv_obj_set_style_text_font(ph_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ph_title, lv_color_hex(0x003366), 0);
    lv_obj_set_pos(ph_title, 50, 150);
    
    // pH buffer buttons
    btn_ph_4 = create_button_with_label(sensor_cal_screen, "pH 4.0", 50, 180, 100, 40);
    lv_obj_add_event_cb(btn_ph_4, btn_ph_calibrate_event_cb, LV_EVENT_CLICKED, (void*)4);
    lv_obj_set_style_bg_color(btn_ph_4, lv_color_hex(0xFF6666), 0);
    
    btn_ph_7 = create_button_with_label(sensor_cal_screen, "pH 7.0", 160, 180, 100, 40);
    lv_obj_add_event_cb(btn_ph_7, btn_ph_calibrate_event_cb, LV_EVENT_CLICKED, (void*)7);
    lv_obj_set_style_bg_color(btn_ph_7, lv_color_hex(0x00AA00), 0);
    
    btn_ph_10 = create_button_with_label(sensor_cal_screen, "pH 10.0", 270, 180, 100, 40);
    lv_obj_add_event_cb(btn_ph_10, btn_ph_calibrate_event_cb, LV_EVENT_CLICKED, (void*)10);
    lv_obj_set_style_bg_color(btn_ph_10, lv_color_hex(0x0066FF), 0);
    
    // Custom pH input
    lv_obj_t *ph_custom_label = lv_label_create(sensor_cal_screen);
    lv_label_set_text(ph_custom_label, "Custom:");
    lv_obj_set_style_text_font(ph_custom_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ph_custom_label, 50, 235);
    
    ph_buffer_input = lv_textarea_create(sensor_cal_screen);
    lv_obj_set_size(ph_buffer_input, 80, 35);
    lv_obj_set_pos(ph_buffer_input, 110, 230);
    lv_textarea_set_text(ph_buffer_input, "7.0");
    lv_textarea_set_one_line(ph_buffer_input, true);
    lv_textarea_set_accepted_chars(ph_buffer_input, "0123456789.");
    
    btn_ph_custom = create_button_with_label(sensor_cal_screen, "Calibrate", 200, 230, 80, 35);
    lv_obj_add_event_cb(btn_ph_custom, btn_ph_custom_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_ph_custom, lv_color_hex(0x0066CC), 0);
    
    btn_clear_ph = create_button_with_label(sensor_cal_screen, "Clear pH Cal", 290, 230, 100, 35);
    lv_obj_add_event_cb(btn_clear_ph, btn_clear_ph_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_clear_ph, lv_color_hex(0xFF6600), 0);
    
    // ORP Calibration Section
    lv_obj_t *orp_title = lv_label_create(sensor_cal_screen);
    lv_label_set_text(orp_title, "ORP Calibration:");
    lv_obj_set_style_text_font(orp_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(orp_title, lv_color_hex(0x003366), 0);
    lv_obj_set_pos(orp_title, 50, 285);
    
    // ORP standard buttons
    btn_orp_225 = create_button_with_label(sensor_cal_screen, "225mV", 50, 315, 100, 40);
    lv_obj_add_event_cb(btn_orp_225, btn_orp_calibrate_event_cb, LV_EVENT_CLICKED, (void*)225);
    lv_obj_set_style_bg_color(btn_orp_225, lv_color_hex(0x9933CC), 0);
    
    btn_orp_470 = create_button_with_label(sensor_cal_screen, "470mV", 160, 315, 100, 40);
    lv_obj_add_event_cb(btn_orp_470, btn_orp_calibrate_event_cb, LV_EVENT_CLICKED, (void*)470);
    lv_obj_set_style_bg_color(btn_orp_470, lv_color_hex(0xCC3399), 0);
    
    // Custom ORP input
    lv_obj_t *orp_custom_label = lv_label_create(sensor_cal_screen);
    lv_label_set_text(orp_custom_label, "Custom:");
    lv_obj_set_style_text_font(orp_custom_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(orp_custom_label, 50, 370);
    
    orp_standard_input = lv_textarea_create(sensor_cal_screen);
    lv_obj_set_size(orp_standard_input, 80, 35);
    lv_obj_set_pos(orp_standard_input, 110, 365);
    lv_textarea_set_text(orp_standard_input, "470");
    lv_textarea_set_one_line(orp_standard_input, true);
    lv_textarea_set_accepted_chars(orp_standard_input, "0123456789.-");
    
    btn_orp_custom = create_button_with_label(sensor_cal_screen, "Calibrate", 200, 365, 80, 35);
    lv_obj_add_event_cb(btn_orp_custom, btn_orp_custom_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_orp_custom, lv_color_hex(0x0066CC), 0);
    
    btn_clear_orp = create_button_with_label(sensor_cal_screen, "Clear ORP Cal", 290, 365, 100, 35);
    lv_obj_add_event_cb(btn_clear_orp, btn_clear_orp_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_clear_orp, lv_color_hex(0xFF6600), 0);
    
    // Status display
    calibration_status_label = lv_label_create(sensor_cal_screen);
    lv_label_set_text(calibration_status_label, "Ready for sensor calibration");
    lv_obj_set_style_text_font(calibration_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(calibration_status_label, 50, 420);
    
    // Back button
    btn_back = create_button_with_label(sensor_cal_screen, "← Back to Dashboard", 550, 420, 180, 40);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x666666), 0);
    
    // Instructions panel
    lv_obj_t *instructions_panel = lv_obj_create(sensor_cal_screen);
    lv_obj_set_size(instructions_panel, 300, 120);
    lv_obj_set_pos(instructions_panel, 450, 180);
    lv_obj_set_style_bg_color(instructions_panel, lv_color_hex(0xF0F8FF), 0);
    lv_obj_set_style_border_color(instructions_panel, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(instructions_panel, 2, 0);
    lv_obj_set_style_radius(instructions_panel, 8, 0);
    
    lv_obj_t *instructions_title = lv_label_create(instructions_panel);
    lv_label_set_text(instructions_title, "Calibration Instructions:");
    lv_obj_set_style_text_font(instructions_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instructions_title, lv_color_hex(0x003366), 0);
    lv_obj_set_pos(instructions_title, 10, 10);
    
    lv_obj_t *instructions_text = lv_label_create(instructions_panel);
    lv_label_set_text(instructions_text, "1. Rinse probe with distilled water\n"
                                        "2. Place in calibration solution\n"
                                        "3. Wait for stable reading\n"
                                        "4. Press calibration button\n"
                                        "5. Repeat for multiple points");
    lv_obj_set_style_text_font(instructions_text, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(instructions_text, lv_color_hex(0x003366), 0);
    lv_obj_set_pos(instructions_text, 10, 35);
    
    ESP_LOGI(TAG, "Sensor calibration screen created");
}

void screen_sensor_calibration_update(void)
{
    if (sensor_cal_screen == NULL) return;
    
    // Update current sensor readings
    char ph_text[32];
    snprintf(ph_text, sizeof(ph_text), "pH: %.2f", g_system_data.ph);
    lv_label_set_text(current_ph_label, ph_text);
    
    char orp_text[32];
    snprintf(orp_text, sizeof(orp_text), "ORP: %.0fmV", g_system_data.orp);
    lv_label_set_text(current_orp_label, orp_text);
    
    // Update reading colors based on sensor health
    if (g_system_data.sensors_healthy) {
        lv_obj_set_style_text_color(current_ph_label, lv_color_hex(0x0066CC), 0);
        lv_obj_set_style_text_color(current_orp_label, lv_color_hex(0x0066CC), 0);
    } else {
        lv_obj_set_style_text_color(current_ph_label, lv_color_hex(0xFF6600), 0);
        lv_obj_set_style_text_color(current_orp_label, lv_color_hex(0xFF6600), 0);
    }
    
    // Disable calibration if sensors are not healthy
    if (!g_system_data.sensors_healthy) {
        lv_obj_add_state(btn_ph_4, LV_STATE_DISABLED);
        lv_obj_add_state(btn_ph_7, LV_STATE_DISABLED);
        lv_obj_add_state(btn_ph_10, LV_STATE_DISABLED);
        lv_obj_add_state(btn_ph_custom, LV_STATE_DISABLED);
        lv_obj_add_state(btn_orp_225, LV_STATE_DISABLED);
        lv_obj_add_state(btn_orp_470, LV_STATE_DISABLED);
        lv_obj_add_state(btn_orp_custom, LV_STATE_DISABLED);
        
        if (strcmp(lv_label_get_text(calibration_status_label), "Sensors not healthy - calibration disabled") != 0) {
            lv_label_set_text(calibration_status_label, "Sensors not healthy - calibration disabled");
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFF0000), 0);
        }
    } else {
        lv_obj_clear_state(btn_ph_4, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_ph_7, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_ph_10, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_ph_custom, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_orp_225, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_orp_470, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_orp_custom, LV_STATE_DISABLED);
        
        if (strcmp(lv_label_get_text(calibration_status_label), "Sensors not healthy - calibration disabled") == 0) {
            lv_label_set_text(calibration_status_label, "Ready for sensor calibration");
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
        }
    }
}

lv_obj_t* get_sensor_calibration_screen(void)
{
    return sensor_cal_screen;
}
