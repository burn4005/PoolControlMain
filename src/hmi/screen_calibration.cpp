#include "hmi_main.h"

static const char *TAG = "SCREEN_CALIBRATION";

// Calibration screen objects
static lv_obj_t *calibration_screen = NULL;
static lv_obj_t *target_volume_input = NULL;
static lv_obj_t *actual_volume_input = NULL;
static lv_obj_t *calibration_status_label = NULL;
static lv_obj_t *pump_status_label = NULL;

// Buttons
static lv_obj_t *btn_start_calibration = NULL;
static lv_obj_t *btn_complete_calibration = NULL;
static lv_obj_t *btn_clear_calibration = NULL;
static lv_obj_t *btn_get_status = NULL;
static lv_obj_t *btn_back = NULL;

// Calibration state
static bool calibration_in_progress = false;

// Button event handlers
static void btn_start_calibration_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char* target_text = lv_textarea_get_text(target_volume_input);
        float target_volume = atof(target_text);
        
        if (target_volume > 0 && target_volume <= 1000) {
            // Send pump calibration start command
            uint8_t cmd_data[5];
            cmd_data[0] = 0x07; // HMI_CMD_PUMP_CAL_START
            memcpy(&cmd_data[1], &target_volume, sizeof(float));
            
            send_command("pump_cal_start", "target_volume", target_volume);
            
            calibration_in_progress = true;
            lv_obj_add_state(btn_start_calibration, LV_STATE_DISABLED);
            lv_obj_clear_state(btn_complete_calibration, LV_STATE_DISABLED);
            lv_obj_clear_state(actual_volume_input, LV_STATE_DISABLED);
            
            lv_label_set_text(calibration_status_label, "Pump dispensing... Measure actual volume");
            
            ESP_LOGI(TAG, "Started pump calibration with %.2fml target", target_volume);
        } else {
            lv_label_set_text(calibration_status_label, "Invalid target volume (1-1000ml)");
        }
    }
}

static void btn_complete_calibration_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char* actual_text = lv_textarea_get_text(actual_volume_input);
        float actual_volume = atof(actual_text);
        
        if (actual_volume > 0 && actual_volume <= 1000) {
            // Send pump calibration complete command
            send_command("pump_cal_complete", "actual_volume", actual_volume);
            
            calibration_in_progress = false;
            lv_obj_clear_state(btn_start_calibration, LV_STATE_DISABLED);
            lv_obj_add_state(btn_complete_calibration, LV_STATE_DISABLED);
            lv_obj_add_state(actual_volume_input, LV_STATE_DISABLED);
            
            lv_label_set_text(calibration_status_label, "Calibration completed successfully!");
            lv_textarea_set_text(actual_volume_input, "");
            
            ESP_LOGI(TAG, "Completed pump calibration with %.2fml actual", actual_volume);
        } else {
            lv_label_set_text(calibration_status_label, "Invalid actual volume (1-1000ml)");
        }
    }
}

static void btn_clear_calibration_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Send pump calibration clear command
        send_command("pump_cal_clear", "", 0);
        
        calibration_in_progress = false;
        lv_obj_clear_state(btn_start_calibration, LV_STATE_DISABLED);
        lv_obj_add_state(btn_complete_calibration, LV_STATE_DISABLED);
        lv_obj_add_state(actual_volume_input, LV_STATE_DISABLED);
        
        lv_label_set_text(calibration_status_label, "Calibration cleared - factory defaults restored");
        lv_textarea_set_text(actual_volume_input, "");
        
        ESP_LOGI(TAG, "Cleared pump calibration");
    }
}

static void btn_get_status_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Send pump status request command
        send_command("pump_cal_status", "", 0);
        ESP_LOGI(TAG, "Requested pump calibration status");
    }
}

static void btn_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_DASHBOARD);
    }
}

void screen_calibration_create(void)
{
    ESP_LOGI(TAG, "Creating calibration screen");
    
    calibration_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(calibration_screen, lv_color_hex(0xE8F4FD), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(calibration_screen);
    lv_label_set_text(title, "Pump Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x003366), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Instructions card
    lv_obj_t *instructions_card = create_status_card(calibration_screen, "Instructions", 50, 50, 700, 100);
    
    lv_obj_t *instructions_text = lv_label_create(instructions_card);
    lv_label_set_text(instructions_text, "1. Enter target volume and start calibration\n"
                                        "2. Measure actual volume dispensed\n"
                                        "3. Enter actual volume and complete calibration");
    lv_obj_set_style_text_font(instructions_text, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(instructions_text, 20, 35);
    
    // Target volume input
    lv_obj_t *target_label = lv_label_create(calibration_screen);
    lv_label_set_text(target_label, "Target Volume (ml):");
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(target_label, 50, 170);
    
    target_volume_input = lv_textarea_create(calibration_screen);
    lv_obj_set_size(target_volume_input, 150, 40);
    lv_obj_set_pos(target_volume_input, 200, 165);
    lv_textarea_set_text(target_volume_input, "100");
    lv_textarea_set_one_line(target_volume_input, true);
    lv_textarea_set_accepted_chars(target_volume_input, "0123456789.");
    
    // Start calibration button
    btn_start_calibration = create_button_with_label(calibration_screen, "Start Calibration", 370, 165, 150, 40);
    lv_obj_add_event_cb(btn_start_calibration, btn_start_calibration_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_start_calibration, lv_color_hex(0x0066CC), 0);
    
    // Actual volume input
    lv_obj_t *actual_label = lv_label_create(calibration_screen);
    lv_label_set_text(actual_label, "Actual Volume (ml):");
    lv_obj_set_style_text_font(actual_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(actual_label, 50, 230);
    
    actual_volume_input = lv_textarea_create(calibration_screen);
    lv_obj_set_size(actual_volume_input, 150, 40);
    lv_obj_set_pos(actual_volume_input, 200, 225);
    lv_textarea_set_one_line(actual_volume_input, true);
    lv_textarea_set_accepted_chars(actual_volume_input, "0123456789.");
    lv_obj_add_state(actual_volume_input, LV_STATE_DISABLED);
    
    // Complete calibration button
    btn_complete_calibration = create_button_with_label(calibration_screen, "Complete", 370, 225, 150, 40);
    lv_obj_add_event_cb(btn_complete_calibration, btn_complete_calibration_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_complete_calibration, lv_color_hex(0x00AA00), 0);
    lv_obj_add_state(btn_complete_calibration, LV_STATE_DISABLED);
    
    // Status display
    calibration_status_label = lv_label_create(calibration_screen);
    lv_label_set_text(calibration_status_label, "Ready to calibrate pump volume");
    lv_obj_set_style_text_font(calibration_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(calibration_status_label, 50, 290);
    
    // Pump status display
    pump_status_label = lv_label_create(calibration_screen);
    lv_label_set_text(pump_status_label, "Pump Status: Unknown");
    lv_obj_set_style_text_font(pump_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(pump_status_label, 50, 320);
    
    // Control buttons
    btn_clear_calibration = create_button_with_label(calibration_screen, "Clear Calibration", 50, 360, 150, 40);
    lv_obj_add_event_cb(btn_clear_calibration, btn_clear_calibration_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_clear_calibration, lv_color_hex(0xFF6600), 0);
    
    btn_get_status = create_button_with_label(calibration_screen, "Get Status", 220, 360, 150, 40);
    lv_obj_add_event_cb(btn_get_status, btn_get_status_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_get_status, lv_color_hex(0x666666), 0);
    
    // Back button
    btn_back = create_button_with_label(calibration_screen, "← Back to Dashboard", 550, 360, 180, 40);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x666666), 0);
    
    // Warning panel
    lv_obj_t *warning_panel = lv_obj_create(calibration_screen);
    lv_obj_set_size(warning_panel, 680, 60);
    lv_obj_set_pos(warning_panel, 60, 420);
    lv_obj_set_style_bg_color(warning_panel, lv_color_hex(0xFFF8DC), 0);
    lv_obj_set_style_border_color(warning_panel, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_border_width(warning_panel, 2, 0);
    lv_obj_set_style_radius(warning_panel, 8, 0);
    
    lv_obj_t *warning_text = lv_label_create(warning_panel);
    lv_label_set_text(warning_text, "⚠️ WARNING: Ensure pump is primed and acid lines are properly connected.\n"
                                   "Use measuring cylinder for accurate volume measurement.");
    lv_obj_set_style_text_font(warning_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(warning_text, lv_color_hex(0x996600), 0);
    lv_obj_align(warning_text, LV_ALIGN_CENTER, 0, 0);
    
    ESP_LOGI(TAG, "Calibration screen created");
}

void screen_calibration_update(void)
{
    if (calibration_screen == NULL) return;
    
    // Update pump status display
    const char* pump_status_strings[] = {"OFF", "LOW SPEED", "MEDIUM SPEED", "HIGH SPEED", "ON BUT STOPPED"};
    char pump_status_text[64];
    
    if (g_system_data.pump_status >= 0 && g_system_data.pump_status < 5) {
        snprintf(pump_status_text, sizeof(pump_status_text), "Pump Status: %s (%.2fA)", 
                 pump_status_strings[g_system_data.pump_status], g_system_data.pump_current);
    } else {
        snprintf(pump_status_text, sizeof(pump_status_text), "Pump Status: UNKNOWN");
    }
    
    lv_label_set_text(pump_status_label, pump_status_text);
    
    // Update pump status color
    if (g_system_data.pump_healthy) {
        lv_obj_set_style_text_color(pump_status_label, lv_color_hex(0x00AA00), 0);
    } else {
        lv_obj_set_style_text_color(pump_status_label, lv_color_hex(0xFF0000), 0);
    }
    
    // Disable calibration if pump is not healthy
    if (!g_system_data.pump_healthy && !calibration_in_progress) {
        lv_obj_add_state(btn_start_calibration, LV_STATE_DISABLED);
        lv_label_set_text(calibration_status_label, "Pump not healthy - calibration disabled");
        lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0xFF0000), 0);
    } else if (!calibration_in_progress) {
        lv_obj_clear_state(btn_start_calibration, LV_STATE_DISABLED);
        if (strcmp(lv_label_get_text(calibration_status_label), "Pump not healthy - calibration disabled") == 0) {
            lv_label_set_text(calibration_status_label, "Ready to calibrate pump volume");
            lv_obj_set_style_text_color(calibration_status_label, lv_color_hex(0x0066CC), 0);
        }
    }
}

lv_obj_t* get_calibration_screen(void)
{
    return calibration_screen;
}
