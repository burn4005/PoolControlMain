#include "hmi_main.h"

static const char *TAG = "SCREEN_LIGHTING";

// Lighting screen objects
static lv_obj_t *lighting_screen = NULL;
static lv_obj_t *current_mode_label = NULL;
static lv_obj_t *runtime_label = NULL;

// Color buttons
static lv_obj_t *btn_blue = NULL;
static lv_obj_t *btn_pink = NULL;
static lv_obj_t *btn_red = NULL;
static lv_obj_t *btn_yellow = NULL;
static lv_obj_t *btn_green = NULL;
static lv_obj_t *btn_cyan = NULL;
static lv_obj_t *btn_white = NULL;
static lv_obj_t *btn_off = NULL;

// Mode buttons
static lv_obj_t *btn_mode1 = NULL;
static lv_obj_t *btn_mode2 = NULL;
static lv_obj_t *btn_mode3 = NULL;
static lv_obj_t *btn_mode4 = NULL;
static lv_obj_t *btn_brightness = NULL;

// Navigation
static lv_obj_t *btn_back = NULL;

// Light mode button event handlers
static void btn_light_mode_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int mode = (int)(intptr_t)lv_event_get_user_data(e);
        
        // Send light mode command to main controller
        char mode_str[16];
        const char* mode_names[] = {
            "off", "blue", "pink", "red", "yellow", 
            "green", "cyan", "white", "mode1", "mode2", 
            "mode3", "mode4", "brightness"
        };
        
        if (mode >= 0 && mode < 13) {
            send_command_string("set_light_mode", "mode", mode_names[mode]);
            ESP_LOGI(TAG, "Light mode changed to: %s", mode_names[mode]);
        }
    }
}

static void btn_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        gui_manager_switch_screen(SCREEN_DASHBOARD);
    }
}

void screen_lighting_create(void)
{
    ESP_LOGI(TAG, "Creating lighting screen");
    
    lighting_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(lighting_screen, lv_color_hex(0xE8F4FD), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(lighting_screen);
    lv_label_set_text(title, "Pool Lighting Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x003366), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current status card
    lv_obj_t *status_card = create_status_card(lighting_screen, "Current Status", 50, 50, 700, 80);
    
    current_mode_label = lv_label_create(status_card);
    lv_label_set_text(current_mode_label, "Current Mode: WHITE");
    lv_obj_set_style_text_font(current_mode_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(current_mode_label, lv_color_hex(0x0066CC), 0);
    lv_obj_set_pos(current_mode_label, 20, 35);
    
    runtime_label = lv_label_create(status_card);
    lv_label_set_text(runtime_label, "Runtime: 1h 23m");
    lv_obj_set_style_text_font(runtime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(runtime_label, 400, 40);
    
    // Color selection section
    lv_obj_t *color_title = lv_label_create(lighting_screen);
    lv_label_set_text(color_title, "Color Selection:");
    lv_obj_set_style_text_font(color_title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(color_title, 50, 150);
    
    // First row of color buttons
    btn_blue = create_button_with_label(lighting_screen, "Blue\n250ms", 50, 180, 120, 60);
    lv_obj_add_event_cb(btn_blue, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_set_style_bg_color(btn_blue, lv_color_hex(0x0066FF), 0);
    
    btn_pink = create_button_with_label(lighting_screen, "Pink\n300ms", 190, 180, 120, 60);
    lv_obj_add_event_cb(btn_pink, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)2);
    lv_obj_set_style_bg_color(btn_pink, lv_color_hex(0xFF66CC), 0);
    
    btn_red = create_button_with_label(lighting_screen, "Red\n350ms", 330, 180, 120, 60);
    lv_obj_add_event_cb(btn_red, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)3);
    lv_obj_set_style_bg_color(btn_red, lv_color_hex(0xFF0000), 0);
    
    btn_yellow = create_button_with_label(lighting_screen, "Yellow\n400ms", 470, 180, 120, 60);
    lv_obj_add_event_cb(btn_yellow, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)4);
    lv_obj_set_style_bg_color(btn_yellow, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_color(btn_yellow, lv_color_hex(0x000000), 0);
    
    // Second row of color buttons
    btn_green = create_button_with_label(lighting_screen, "Green\n450ms", 50, 260, 120, 60);
    lv_obj_add_event_cb(btn_green, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)5);
    lv_obj_set_style_bg_color(btn_green, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_color(btn_green, lv_color_hex(0x000000), 0);
    
    btn_cyan = create_button_with_label(lighting_screen, "Cyan\n500ms", 190, 260, 120, 60);
    lv_obj_add_event_cb(btn_cyan, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)6);
    lv_obj_set_style_bg_color(btn_cyan, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_color(btn_cyan, lv_color_hex(0x000000), 0);
    
    btn_white = create_button_with_label(lighting_screen, "White\n550ms", 330, 260, 120, 60);
    lv_obj_add_event_cb(btn_white, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)7);
    lv_obj_set_style_bg_color(btn_white, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(btn_white, lv_color_hex(0x000000), 0);
    
    btn_off = create_button_with_label(lighting_screen, "OFF", 470, 260, 120, 60);
    lv_obj_add_event_cb(btn_off, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0x666666), 0);
    
    // Special modes section
    lv_obj_t *modes_title = lv_label_create(lighting_screen);
    lv_label_set_text(modes_title, "Special Modes:");
    lv_obj_set_style_text_font(modes_title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(modes_title, 50, 340);
    
    btn_mode1 = create_button_with_label(lighting_screen, "Mode 1\n600ms", 50, 370, 120, 60);
    lv_obj_add_event_cb(btn_mode1, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)8);
    lv_obj_set_style_bg_color(btn_mode1, lv_color_hex(0x9933CC), 0);
    
    btn_mode2 = create_button_with_label(lighting_screen, "Mode 2\n650ms", 190, 370, 120, 60);
    lv_obj_add_event_cb(btn_mode2, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)9);
    lv_obj_set_style_bg_color(btn_mode2, lv_color_hex(0xCC3399), 0);
    
    btn_mode3 = create_button_with_label(lighting_screen, "Mode 3\n700ms", 330, 370, 120, 60);
    lv_obj_add_event_cb(btn_mode3, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)10);
    lv_obj_set_style_bg_color(btn_mode3, lv_color_hex(0x3399CC), 0);
    
    btn_mode4 = create_button_with_label(lighting_screen, "Mode 4\n750ms", 470, 370, 120, 60);
    lv_obj_add_event_cb(btn_mode4, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)11);
    lv_obj_set_style_bg_color(btn_mode4, lv_color_hex(0x99CC33), 0);
    
    btn_brightness = create_button_with_label(lighting_screen, "Brightness\n900ms", 610, 370, 120, 60);
    lv_obj_add_event_cb(btn_brightness, btn_light_mode_event_cb, LV_EVENT_CLICKED, (void*)12);
    lv_obj_set_style_bg_color(btn_brightness, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_color(btn_brightness, lv_color_hex(0x000000), 0);
    
    // Back button
    btn_back = create_button_with_label(lighting_screen, "← Back to Dashboard", 600, 50, 180, 50);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x666666), 0);
    
    // Information panel
    lv_obj_t *info_panel = lv_obj_create(lighting_screen);
    lv_obj_set_size(info_panel, 680, 60);
    lv_obj_set_pos(info_panel, 60, 450);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0xF0F8FF), 0);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(info_panel, 2, 0);
    lv_obj_set_style_radius(info_panel, 8, 0);
    
    lv_obj_t *info_text = lv_label_create(info_panel);
    lv_label_set_text(info_text, "Light modes are selected by turning lights OFF for the specified duration.\n"
                                 "The system will automatically turn lights back ON in the selected mode.");
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info_text, lv_color_hex(0x003366), 0);
    lv_obj_align(info_text, LV_ALIGN_CENTER, 0, 0);
    
    ESP_LOGI(TAG, "Lighting screen created");
}

void screen_lighting_update(void)
{
    if (lighting_screen == NULL) return;
    
    // Update current mode display
    char mode_text[64];
    if (g_system_data.light_relay_on) {
        snprintf(mode_text, sizeof(mode_text), "Current Mode: %s", 
                 get_light_mode_string(g_system_data.current_light_mode));
        lv_obj_set_style_text_color(current_mode_label, lv_color_hex(0x0066CC), 0);
    } else {
        strcpy(mode_text, "Current Mode: OFF");
        lv_obj_set_style_text_color(current_mode_label, lv_color_hex(0x666666), 0);
    }
    lv_label_set_text(current_mode_label, mode_text);
    
    // Update runtime display
    if (g_system_data.light_relay_on) {
        char runtime_text[32];
        format_time_string(runtime_text, sizeof(runtime_text), g_system_data.timestamp);
        char full_runtime_text[64];
        snprintf(full_runtime_text, sizeof(full_runtime_text), "Runtime: %s", runtime_text);
        lv_label_set_text(runtime_label, full_runtime_text);
    } else {
        lv_label_set_text(runtime_label, "Runtime: --");
    }
    
    // Highlight current mode button
    lv_obj_t* buttons[] = {
        btn_off, btn_blue, btn_pink, btn_red, btn_yellow,
        btn_green, btn_cyan, btn_white, btn_mode1, btn_mode2,
        btn_mode3, btn_mode4, btn_brightness
    };
    
    // Reset all button borders
    for (int i = 0; i < 13; i++) {
        lv_obj_set_style_border_width(buttons[i], 0, 0);
    }
    
    // Highlight current mode
    int current_mode = g_system_data.light_relay_on ? g_system_data.current_light_mode : 0;
    if (current_mode >= 0 && current_mode < 13) {
        lv_obj_set_style_border_width(buttons[current_mode], 4, 0);
        lv_obj_set_style_border_color(buttons[current_mode], lv_color_hex(0xFFFFFF), 0);
    }
}

lv_obj_t* get_lighting_screen(void)
{
    return lighting_screen;
}
