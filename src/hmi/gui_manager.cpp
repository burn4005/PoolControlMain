#include "hmi_main.h"

static const char *TAG = "GUI_MANAGER";

// Current screen tracking
static screen_id_t current_screen = SCREEN_DASHBOARD;
static lv_obj_t *screens[SCREEN_COUNT] = {NULL};

// Defined in screen files
extern lv_obj_t* get_dashboard_screen(void);
extern lv_obj_t* get_lighting_screen(void);

void gui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing GUI Manager...");
    
    // Create all screens
    screen_dashboard_create();
    screens[SCREEN_DASHBOARD] = get_dashboard_screen();
    
    screen_lighting_create();
    screens[SCREEN_LIGHTING] = get_lighting_screen();
    
    // TODO: Create other screens when implemented
    // screen_manual_create();
    // screen_settings_create();
    // screen_alarms_create();
    // screen_calibration_create();
    // screen_data_create();
    
    // Load dashboard screen initially
    lv_disp_load_scr(screens[SCREEN_DASHBOARD]);
    current_screen = SCREEN_DASHBOARD;
    
    ESP_LOGI(TAG, "GUI Manager initialized - Dashboard screen loaded");
}

void gui_manager_switch_screen(screen_id_t screen_id)
{
    if (screen_id >= SCREEN_COUNT) {
        ESP_LOGW(TAG, "Invalid screen ID: %d", screen_id);
        return;
    }
    
    if (screens[screen_id] == NULL) {
        ESP_LOGW(TAG, "Screen %d not implemented yet", screen_id);
        return;
    }
    
    if (screen_id == current_screen) {
        ESP_LOGD(TAG, "Already on screen %d", screen_id);
        return;
    }
    
    ESP_LOGI(TAG, "Switching from screen %d to screen %d", current_screen, screen_id);
    
    // Load the new screen
    lv_disp_load_scr(screens[screen_id]);
    current_screen = screen_id;
    
    // Update the new screen with current data
    gui_manager_update_data();
}

screen_id_t gui_manager_get_current_screen(void)
{
    return current_screen;
}

void gui_manager_update_data(void)
{
    // Update the current screen with latest data
    switch (current_screen) {
        case SCREEN_DASHBOARD:
            screen_dashboard_update();
            break;
            
        case SCREEN_LIGHTING:
            screen_lighting_update();
            break;
            
        case SCREEN_MANUAL:
            // screen_manual_update();
            break;
            
        case SCREEN_SETTINGS:
            // screen_settings_update();
            break;
            
        case SCREEN_ALARMS:
            // screen_alarms_update();
            break;
            
        case SCREEN_CALIBRATION:
            // screen_calibration_update();
            break;
            
        case SCREEN_DATA:
            // screen_data_update();
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown screen ID: %d", current_screen);
            break;
    }
}

// Screen create/update functions are defined in their respective screen_*.cpp files
