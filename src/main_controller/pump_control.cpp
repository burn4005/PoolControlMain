#include "pool_controller.h"

static const char *TAG = "PUMP_CONTROL";

static uint64_t pump_alarm_timer = 0;

void pump_control_init(void)
{
    ESP_LOGI(TAG, "Initializing Pump Control...");
    
    // Configure pump relay pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PUMP_RELAY_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Initialize pump relay to OFF
    gpio_set_level(PUMP_RELAY_PIN, 0);
    g_state.pump_relay_on = false;
    g_state.pump_status = PUMP_OFF;
    g_state.pump_healthy = false;
    
    ESP_LOGI(TAG, "Pump Control Initialized");
}

void update_pump_status(void)
{
    pump_status_t new_status = detect_pump_status();
    
    // Update pump health based on status
    bool new_pump_healthy = (new_status != PUMP_OFF && new_status != PUMP_ON_BUT_STOPPED);
    
    if (new_pump_healthy != g_state.pump_healthy) {
        g_state.pump_healthy = new_pump_healthy;
        log_event(new_pump_healthy ? "Pump health: HEALTHY" : "Pump health: UNHEALTHY");
        
        if (new_pump_healthy) {
            g_state.pump_start_time = get_timestamp_ms();
        }
    }
    
    if (new_status != g_state.pump_status) {
        ESP_LOGI(TAG, "Pump status: %s -> %s (Current: %.2fA)", 
                 get_pump_status_string(g_state.pump_status),
                 get_pump_status_string(new_status),
                 g_state.pump_current);
        
        g_state.pump_status = new_status;
        g_state.pump_status_change_time = get_timestamp_ms();
        pump_alarm_timer = 0;
        
        // Clear alarm if pump is now healthy
        if (g_state.pump_healthy && g_state.pump_alarm_active) {
            clear_alarm("PUMP_UNHEALTHY");
            g_state.pump_alarm_active = false;
        }
    }
    
    // Check for unhealthy pump alarm
    if (!g_state.pump_healthy && g_state.pump_relay_on) {
        if (pump_alarm_timer == 0) {
            pump_alarm_timer = get_timestamp_ms();
        } else if ((get_timestamp_ms() - pump_alarm_timer) > 60000 && !g_state.pump_alarm_active) {
            char alarm_msg[128];
            snprintf(alarm_msg, sizeof(alarm_msg), "Pump relay ON but %s - Check pump/power", 
                     get_pump_status_string(g_state.pump_status));
            raise_alarm("PUMP_UNHEALTHY", alarm_msg);
            g_state.pump_alarm_active = true;
        }
    } else {
        pump_alarm_timer = 0;
    }
}

pump_status_t detect_pump_status(void)
{
    if (!g_state.pump_relay_on) {
        return PUMP_OFF;
    }
    
    // Relay is ON, check current against user-configured thresholds
    float current = g_state.pump_current;
    
    if (current <= g_config.pump_thresholds.stopped_max) {
        return PUMP_ON_BUT_STOPPED;  // UNHEALTHY STATE
    } else if (current < g_config.pump_thresholds.low_speed_min) {
        return PUMP_ON_BUT_STOPPED;  // Still unhealthy - between stopped and low
    } else if (current < g_config.pump_thresholds.medium_speed_min) {
        return PUMP_LOW_SPEED;       // HEALTHY STATE
    } else if (current < g_config.pump_thresholds.high_speed_min) {
        return PUMP_MEDIUM_SPEED;    // HEALTHY STATE
    } else {
        return PUMP_HIGH_SPEED;      // HEALTHY STATE
    }
}

bool is_pump_healthy(void)
{
    return g_state.pump_healthy;
}

void set_pump_relay(bool state)
{
    gpio_set_level(PUMP_RELAY_PIN, state ? 1 : 0);
    g_state.pump_relay_on = state;
    
    if (state) {
        ESP_LOGI(TAG, "Pump relay turned ON");
        log_event("Pump relay turned ON");
    } else {
        ESP_LOGI(TAG, "Pump relay turned OFF");
        log_event("Pump relay turned OFF");
        
        // Store pH reading for next correction calculation
        if (g_state.sensors_healthy) {
            g_state.last_ph_at_pump_stop = g_state.ph;
        }
        
        // Reset pump state
        g_state.pump_status = PUMP_OFF;
        g_state.pump_healthy = false;
        g_state.sensors_healthy = false;
    }
}

const char* get_pump_status_string(pump_status_t status)
{
    switch (status) {
        case PUMP_OFF: return "OFF";
        case PUMP_LOW_SPEED: return "LOW SPEED";
        case PUMP_MEDIUM_SPEED: return "MEDIUM SPEED";
        case PUMP_HIGH_SPEED: return "HIGH SPEED";
        case PUMP_ON_BUT_STOPPED: return "ON BUT STOPPED";
        default: return "UNKNOWN";
    }
}
