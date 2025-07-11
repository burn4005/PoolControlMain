#include "pool_controller.h"

static const char *TAG = "LIGHT_CONTROL";

// Light timing definitions for different modes
const light_timing_t light_timings[] = {
    {LIGHT_OFF, "Off", 0, "Lights off"},
    {LIGHT_BLUE, "Blue", 1000, "Blue color mode"},
    {LIGHT_PINK, "Pink", 2000, "Pink color mode"},
    {LIGHT_RED, "Red", 3000, "Red color mode"},
    {LIGHT_YELLOW, "Yellow", 4000, "Yellow color mode"},
    {LIGHT_GREEN, "Green", 5000, "Green color mode"},
    {LIGHT_CYAN, "Cyan", 6000, "Cyan color mode"},
    {LIGHT_WHITE, "White", 0, "White color mode"},
    {LIGHT_MODE1, "Mode 1", 7000, "Special mode 1"},
    {LIGHT_MODE2, "Mode 2", 8000, "Special mode 2"},
    {LIGHT_MODE3, "Mode 3", 9000, "Special mode 3"},
    {LIGHT_MODE4, "Mode 4", 10000, "Special mode 4"},
    {LIGHT_BRIGHTNESS, "Brightness", 11000, "Brightness adjustment"}
};

void light_control_init(void)
{
    ESP_LOGI(TAG, "Initializing Light Control...");
    
    // Configure light relay pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LIGHT_RELAY_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Initialize light relay to OFF
    gpio_set_level(LIGHT_RELAY_PIN, 0);
    g_state.light_relay_on = false;
    g_state.current_light_mode = LIGHT_OFF;
    g_state.light_mode_change_pending = false;
    
    ESP_LOGI(TAG, "Light Control Initialized");
}

void set_light_mode(light_mode_t mode)
{
    if (mode == LIGHT_OFF) {
        // Turn lights off
        gpio_set_level(LIGHT_RELAY_PIN, 0);
        g_state.light_relay_on = false;
        g_state.current_light_mode = LIGHT_OFF;
        g_state.light_change_time = get_timestamp_ms();
        ESP_LOGI(TAG, "Pool lights turned OFF");
        log_event("Pool lights turned OFF");
        return;
    }
    
    if (g_state.current_light_mode == LIGHT_OFF) {
        // Lights are currently off, just turn them on to white
        gpio_set_level(LIGHT_RELAY_PIN, 1);
        g_state.light_relay_on = true;
        g_state.current_light_mode = LIGHT_WHITE;
        g_state.light_change_time = get_timestamp_ms();
        ESP_LOGI(TAG, "Pool lights turned ON (White)");
        log_event("Pool lights turned ON (White)");
        
        // If requested mode is not white, schedule the mode change
        if (mode != LIGHT_WHITE) {
            schedule_light_mode_change(mode);
        }
    } else {
        // Lights are on, change to new mode
        schedule_light_mode_change(mode);
    }
}

void schedule_light_mode_change(light_mode_t new_mode)
{
    // Find the timing for the requested mode
    uint16_t off_time = 0;
    const char* mode_name = "Unknown";
    
    for (int i = 0; i < LIGHT_MODE_COUNT; i++) {
        if (light_timings[i].mode == new_mode) {
            off_time = light_timings[i].off_time_ms;
            mode_name = light_timings[i].name;
            break;
        }
    }
    
    if (off_time > 0) {
        // Turn lights off for the specified time
        gpio_set_level(LIGHT_RELAY_PIN, 0);
        g_state.light_relay_on = false;
        
        // Schedule turning them back on
        g_state.light_mode_change_pending = true;
        g_state.light_mode_change_time = get_timestamp_ms() + off_time;
        g_state.pending_light_mode = new_mode;
        
        ESP_LOGI(TAG, "Light mode change: OFF for %ums -> %s", off_time, mode_name);
        
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Light mode change: OFF for %ums -> %s", off_time, mode_name);
        log_event(log_msg);
    }
}

void check_light_mode_change(void)
{
    if (g_state.light_mode_change_pending && 
        get_timestamp_ms() >= g_state.light_mode_change_time) {
        
        // Time to turn lights back on
        gpio_set_level(LIGHT_RELAY_PIN, 1);
        g_state.light_relay_on = true;
        g_state.current_light_mode = g_state.pending_light_mode;
        g_state.light_change_time = get_timestamp_ms();
        g_state.light_mode_change_pending = false;
        
        const char* mode_name = get_light_mode_name(g_state.current_light_mode);
        ESP_LOGI(TAG, "Light mode changed to: %s", mode_name);
        
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "Light mode changed to: %s", mode_name);
        log_event(log_msg);
    }
}

const char* get_light_mode_name(light_mode_t mode)
{
    for (int i = 0; i < LIGHT_MODE_COUNT; i++) {
        if (light_timings[i].mode == mode) {
            return light_timings[i].name;
        }
    }
    return "Unknown";
}

void cycle_brightness(void)
{
    // Use the brightness timing to cycle through brightness levels
    schedule_light_mode_change(LIGHT_BRIGHTNESS);
}

bool are_lights_on(void)
{
    return g_state.light_relay_on;
}

light_mode_t get_current_light_mode(void)
{
    return g_state.current_light_mode;
}

uint64_t get_light_runtime_ms(void)
{
    if (g_state.light_relay_on) {
        return get_timestamp_ms() - g_state.light_change_time;
    }
    return 0;
}
