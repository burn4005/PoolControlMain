#include "pool_controller.h"

static const char *TAG = "POOL_CONTROLLER";

void pool_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing Pool Controller...");
    
    // Set default configuration
    system_config_set_defaults();
    
    // Load configuration from NVS (4MB SPI Flash)
    system_config_load();
    
    // Initialize hardware components
    current_sensors_init();
    atlas_scientific_init();
    pump_control_init();
    chlorinator_control_init();
    light_control_init();
    chemical_dosing_init();
    learning_systems_init();
    hmi_communication_init();
    
    // Initialize WiFi and network services
    wifi_manager_init();
    ntp_sync_init();
    
    // Initialize system state
    memset(&g_state, 0, sizeof(system_state_t));
    g_state.acid_remaining_ml = g_config.acid_bottle_size_ml;
    g_state.last_ph_correction_perc = 100.0f;
    g_state.sensors_healthy = false;
    g_state.current_light_mode = LIGHT_OFF;
    
    // Load runtime state and learning history from NVS
    system_config_load_runtime_state();
    system_config_load_learning_history();
    
    // Print current configuration and runtime state
    system_config_print_current();
    
    // Initialize web server
    web_server_init();
    web_server_start();
    
    ESP_LOGI(TAG, "Pool Controller Initialization Complete - All setpoints restored from 4MB SPI Flash");
}

void pool_controller_task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1 second
    
    ESP_LOGI(TAG, "Pool Controller Task Started");
    
    while (1) {
        // Read sensors (only if pump has been running for 2+ minutes)
        if (g_state.pump_healthy && 
            (esp_timer_get_time() / 1000 - g_state.pump_start_time) >= SENSOR_STABILIZATION_TIME_MS) {
            g_state.sensors_healthy = true;
            g_state.temperature = atlas_read_temperature();
            g_state.ph = atlas_read_ph();
            g_state.orp = atlas_read_orp();
        } else {
            g_state.sensors_healthy = false;
        }
        
        // Read current sensors
        g_state.pump_current = read_pump_current();
        g_state.chlorinator_current = read_chlorinator_current();
        
        // Update equipment status
        update_pump_status();
        manage_duty_cycle();
        check_light_mode_change();
        
        // Chemical dosing (only when sensors are healthy)
        if (g_state.sensors_healthy) {
            perform_base_acid_addition();
            perform_ph_correction();
        }
        
        // ORP learning adjustment (only when pump running and sensors healthy)
        if (g_state.pump_healthy && g_state.sensors_healthy) {
            adjust_chlorinator_for_orp();
        }
        
        // Track chlorinator runtime
        track_chlorinator_runtime();
        
        // Learning system evaluations
        if (g_state.ph_learning_evaluation_pending) {
            evaluate_ph_correction_effectiveness();
        }
        
        if (g_state.orp_learning_evaluation_pending) {
            evaluate_orp_adjustment_effectiveness();
        }
        
        // Send data to HMI
        hmi_send_data();
        
        // Process HMI commands
        hmi_process_commands();
        
        // Send real-time data to web clients
        web_server_send_realtime_data();
        
        // WiFi maintenance tasks
        wifi_manager_maintenance_task();
        
        // Check for low acid alarm
        if (g_state.acid_remaining_ml <= g_config.acid_low_alert_ml && !g_state.acid_low_alarm_active) {
            raise_alarm("ACID_LOW", "Acid level low - refill required");
            g_state.acid_low_alarm_active = true;
        }
        
        // Log system status every 60 seconds
        static uint32_t log_counter = 0;
        if (++log_counter >= 60) {
            log_counter = 0;
            ESP_LOGI(TAG, "Status: Pump=%s, Chlorinator=%s, pH=%.2f, ORP=%.0f, Temp=%.1f°C", 
                     get_pump_status_string(g_state.pump_status),
                     g_state.duty_cycle_active ? "ON" : "OFF",
                     g_state.ph, g_state.orp, g_state.temperature);
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Utility Functions
void log_event(const char* message)
{
    ESP_LOGI(TAG, "EVENT: %s", message);
    // TODO: Store in event log for HMI display
}

void raise_alarm(const char* alarm_id, const char* message)
{
    ESP_LOGW(TAG, "ALARM [%s]: %s", alarm_id, message);
    // TODO: Store alarm in system and notify HMI
}

void clear_alarm(const char* alarm_id)
{
    ESP_LOGI(TAG, "ALARM CLEARED [%s]", alarm_id);
    // TODO: Clear alarm from system and notify HMI
}


float calculate_temperature_compensated_orp(float water_temp)
{
    float temp_offset = (water_temp - 25.0f) * g_config.orp_temp_coefficient;
    return g_config.base_orp_target + temp_offset;
}

float calculate_time_based_orp_target(int current_hour)
{
    // Time-based ORP adjustments
    if (current_hour >= 6 && current_hour < 18) {
        return 0.0f; // Daytime - no adjustment
    } else if (current_hour >= 18 && current_hour < 22) {
        return -50.0f; // Evening - reduce by 50mV
    } else {
        return -100.0f; // Night - reduce by 100mV
    }
}

float calculate_optimal_orp_target(void)
{
    // Get temperature-compensated target
    float temp_compensated_orp = calculate_temperature_compensated_orp(g_state.temperature);
    
    // Get current time for time-based adjustment
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Apply time-based adjustment
    float time_adjustment = calculate_time_based_orp_target(timeinfo.tm_hour);
    
    // Calculate final target with safety limits
    float final_target = temp_compensated_orp + time_adjustment;
    
    // Safety limits (600-800mV)
    if (final_target < 600.0f) final_target = 600.0f;
    if (final_target > 800.0f) final_target = 800.0f;
    
    return final_target;
}
