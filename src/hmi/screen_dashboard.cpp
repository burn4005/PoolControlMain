#include "hmi_main.h"

static const char *TAG = "SCREEN_DASHBOARD";

// Dashboard screen objects
static lv_obj_t *dashboard_screen = NULL;
static lv_obj_t *temp_value = NULL;
static lv_obj_t *ph_value = NULL;
static lv_obj_t *orp_value = NULL;
static lv_obj_t *pump_status_label = NULL;
static lv_obj_t *pump_current_label = NULL;
static lv_obj_t *chlorinator_status_label = NULL;
static lv_obj_t *chlorinator_current_label = NULL;
static lv_obj_t *light_status_label = NULL;
static lv_obj_t *acid_level_bar = NULL;
static lv_obj_t *acid_level_label = NULL;
static lv_obj_t *learning_ph_label = NULL;
static lv_obj_t *learning_orp_label = NULL;

// Navigation buttons
static lv_obj_t *btn_manual = NULL;
static lv_obj_t *btn_lighting = NULL;
static lv_obj_t *btn_settings = NULL;
static lv_obj_t *btn_alarms = NULL;

// Button event handlers
static void btn_manual_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_MANUAL);
    }
}

static void btn_lighting_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_LIGHTING);
    }
}

static void btn_settings_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_SETTINGS);
    }
}

static void btn_alarms_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_ALARMS);
    }
}

void screen_dashboard_create(void)
{
    ESP_LOGI(TAG, "Creating dashboard screen");
    
    dashboard_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dashboard_screen, lv_color_hex(0xE8F4FD), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(dashboard_screen);
    lv_label_set_text(title, "Pool Controller Dashboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x003366), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Water Quality Card
    lv_obj_t *water_card = create_status_card(dashboard_screen, "Water Quality", 10, 50, 250, 180);
    
    temp_value = create_value_display(water_card, "Temperature", "25.4", "°C", 10, 30);
    ph_value = create_value_display(water_card, "pH Level", "7.2", "", 130, 30);
    orp_value = create_value_display(water_card, "ORP", "685", "mV", 70, 100);
    
    // Equipment Status Card
    lv_obj_t *equipment_card = create_status_card(dashboard_screen, "Equipment Status", 270, 50, 250, 180);
    
    // Pump status
    lv_obj_t *pump_label = lv_label_create(equipment_card);
    lv_label_set_text(pump_label, "Pool Pump:");
    lv_obj_set_style_text_font(pump_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(pump_label, 10, 30);
    
    pump_status_label = lv_label_create(equipment_card);
    lv_label_set_text(pump_status_label, "MEDIUM SPEED");
    lv_obj_set_style_text_font(pump_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pump_status_label, lv_color_hex(0x00AA00), 0);
    lv_obj_set_pos(pump_status_label, 10, 50);
    
    pump_current_label = lv_label_create(equipment_card);
    lv_label_set_text(pump_current_label, "Current: 4.8A");
    lv_obj_set_style_text_font(pump_current_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(pump_current_label, 10, 70);
    
    // Chlorinator status
    lv_obj_t *chlorinator_label = lv_label_create(equipment_card);
    lv_label_set_text(chlorinator_label, "Chlorinator:");
    lv_obj_set_style_text_font(chlorinator_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(chlorinator_label, 10, 95);
    
    chlorinator_status_label = lv_label_create(equipment_card);
    lv_label_set_text(chlorinator_status_label, "25% DUTY");
    lv_obj_set_style_text_font(chlorinator_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(chlorinator_status_label, lv_color_hex(0x00AA00), 0);
    lv_obj_set_pos(chlorinator_status_label, 10, 115);
    
    chlorinator_current_label = lv_label_create(equipment_card);
    lv_label_set_text(chlorinator_current_label, "Current: 0.65A");
    lv_obj_set_style_text_font(chlorinator_current_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(chlorinator_current_label, 10, 135);
    
    // Light status
    lv_obj_t *light_label = lv_label_create(equipment_card);
    lv_label_set_text(light_label, "Pool Lights:");
    lv_obj_set_style_text_font(light_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(light_label, 130, 30);
    
    light_status_label = lv_label_create(equipment_card);
    lv_label_set_text(light_status_label, "WHITE");
    lv_obj_set_style_text_font(light_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(light_status_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(light_status_label, 130, 50);
    
    // Chemical System Card
    lv_obj_t *chemical_card = create_status_card(dashboard_screen, "Chemical System", 530, 50, 250, 180);
    
    // Acid level
    lv_obj_t *acid_title = lv_label_create(chemical_card);
    lv_label_set_text(acid_title, "Acid Level:");
    lv_obj_set_style_text_font(acid_title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(acid_title, 10, 30);
    
    acid_level_bar = lv_bar_create(chemical_card);
    lv_obj_set_size(acid_level_bar, 200, 20);
    lv_obj_set_pos(acid_level_bar, 10, 50);
    lv_bar_set_range(acid_level_bar, 0, 5000);
    lv_bar_set_value(acid_level_bar, 4200, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(acid_level_bar, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(acid_level_bar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    
    acid_level_label = lv_label_create(chemical_card);
    lv_label_set_text(acid_level_label, "4.2L / 5.0L");
    lv_obj_set_style_text_font(acid_level_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(acid_level_label, 10, 75);
    
    // Learning system status
    lv_obj_t *learning_title = lv_label_create(chemical_card);
    lv_label_set_text(learning_title, "Learning System:");
    lv_obj_set_style_text_font(learning_title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(learning_title, 10, 105);
    
    learning_ph_label = lv_label_create(chemical_card);
    lv_label_set_text(learning_ph_label, "pH Gain: 15.2%");
    lv_obj_set_style_text_font(learning_ph_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(learning_ph_label, 10, 125);
    
    learning_orp_label = lv_label_create(chemical_card);
    lv_label_set_text(learning_orp_label, "ORP Gain: 12.8%");
    lv_obj_set_style_text_font(learning_orp_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(learning_orp_label, 10, 145);
    
    // Navigation buttons
    btn_manual = create_button_with_label(dashboard_screen, "Manual Control", 50, 250, 150, 50);
    lv_obj_add_event_cb(btn_manual, btn_manual_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_manual, lv_color_hex(0x0066CC), 0);
    
    btn_lighting = create_button_with_label(dashboard_screen, "Pool Lighting", 220, 250, 150, 50);
    lv_obj_add_event_cb(btn_lighting, btn_lighting_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_lighting, lv_color_hex(0x9933CC), 0);
    
    btn_settings = create_button_with_label(dashboard_screen, "Settings", 390, 250, 150, 50);
    lv_obj_add_event_cb(btn_settings, btn_settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x666666), 0);
    
    btn_alarms = create_button_with_label(dashboard_screen, "Alarms", 560, 250, 150, 50);
    lv_obj_add_event_cb(btn_alarms, btn_alarms_event_cb, LV_EVENT_CLICKED, NULL);
    
    // Status bar
    lv_obj_t *status_bar = lv_obj_create(dashboard_screen);
    lv_obj_set_size(status_bar, 780, 30);
    lv_obj_set_pos(status_bar, 10, 440);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(status_bar, 5, 0);
    
    lv_obj_t *status_text = lv_label_create(status_bar);
    lv_label_set_text(status_text, "System Status: All systems operational");
    lv_obj_set_style_text_color(status_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(status_text, &lv_font_montserrat_12, 0);
    lv_obj_align(status_text, LV_ALIGN_CENTER, 0, 0);
    
    ESP_LOGI(TAG, "Dashboard screen created");
}

void screen_dashboard_update(void)
{
    if (dashboard_screen == NULL) return;
    
    // Update sensor values
    update_value_display(temp_value, g_system_data.temperature, "%.1f", "°C");
    update_value_display(ph_value, g_system_data.ph, "%.2f", "");
    update_value_display(orp_value, g_system_data.orp, "%.0f", "mV");
    
    // Update pump status
    lv_label_set_text(pump_status_label, get_pump_status_string(g_system_data.pump_status));
    lv_obj_set_style_text_color(pump_status_label, get_status_color(g_system_data.pump_healthy), 0);
    
    char pump_current_text[32];
    snprintf(pump_current_text, sizeof(pump_current_text), "Current: %.2fA", g_system_data.pump_current);
    lv_label_set_text(pump_current_label, pump_current_text);
    
    // Update chlorinator status
    char chlorinator_status_text[32];
    if (g_system_data.chlorinator_relay_on) {
        snprintf(chlorinator_status_text, sizeof(chlorinator_status_text), "%.0f%% DUTY", g_system_data.chlorinator_duty_cycle);
        lv_obj_set_style_text_color(chlorinator_status_label, lv_color_hex(0x00AA00), 0);
    } else {
        strcpy(chlorinator_status_text, "OFF");
        lv_obj_set_style_text_color(chlorinator_status_label, lv_color_hex(0x666666), 0);
    }
    lv_label_set_text(chlorinator_status_label, chlorinator_status_text);
    
    char chlorinator_current_text[32];
    snprintf(chlorinator_current_text, sizeof(chlorinator_current_text), "Current: %.2fA", g_system_data.chlorinator_current);
    lv_label_set_text(chlorinator_current_label, chlorinator_current_text);
    
    // Update light status
    if (g_system_data.light_relay_on) {
        lv_label_set_text(light_status_label, get_light_mode_string(g_system_data.current_light_mode));
        lv_obj_set_style_text_color(light_status_label, lv_color_hex(0x0066CC), 0);
    } else {
        lv_label_set_text(light_status_label, "OFF");
        lv_obj_set_style_text_color(light_status_label, lv_color_hex(0x666666), 0);
    }
    
    // Update acid level
    lv_bar_set_value(acid_level_bar, (int32_t)g_system_data.acid_remaining_ml, LV_ANIM_OFF);
    
    // Update acid level color based on remaining amount
    if (g_system_data.acid_remaining_ml < 500) {
        lv_obj_set_style_bg_color(acid_level_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    } else if (g_system_data.acid_remaining_ml < 1000) {
        lv_obj_set_style_bg_color(acid_level_bar, lv_color_hex(0xFFAA00), LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_bg_color(acid_level_bar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    }
    
    char acid_text[32];
    format_volume_string(acid_text, sizeof(acid_text), g_system_data.acid_remaining_ml);
    strcat(acid_text, " / 5.0L");
    lv_label_set_text(acid_level_label, acid_text);
    
    // Update learning system
    char ph_learning_text[32];
    snprintf(ph_learning_text, sizeof(ph_learning_text), "pH Gain: %.1f%%", g_system_data.ph_correction_learning_gain);
    lv_label_set_text(learning_ph_label, ph_learning_text);
    
    char orp_learning_text[32];
    snprintf(orp_learning_text, sizeof(orp_learning_text), "ORP Gain: %.1f%%", g_system_data.orp_learning_gain);
    lv_label_set_text(learning_orp_label, orp_learning_text);
    
    // Update alarm button color
    if (g_system_data.pump_alarm_active || g_system_data.chlorinator_alarm_active || g_system_data.acid_low_alarm_active) {
        lv_obj_set_style_bg_color(btn_alarms, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_bg_color(btn_alarms, lv_color_hex(0x00AA00), 0);
    }
}

lv_obj_t* get_dashboard_screen(void)
{
    return dashboard_screen;
}
