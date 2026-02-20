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

    // Initialize Shelly control (requires WiFi)
    shelly_control_init();
    
    // Initialize system state
    memset(&g_state, 0, sizeof(system_state_t));
    g_state.acid_remaining_ml = g_config.acid_bottle_size_ml;
    g_state.last_ph_correction_perc = 100.0f;
    g_state.sensors_healthy = false;
    g_state.emergency_stop_active = false;
    g_state.daily_acid_dosed_ml = 0.0f;
    g_state.daily_acid_reset_day = 0;
    g_state.current_light_mode = LIGHT_OFF;
    g_state.shelly_reachable = false;
    g_state.shelly_alarm_active = false;
    
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
        // Emergency stop check - halt all operations until manually reset
        if (g_state.emergency_stop_active) {
            atlas_stop_dosing();
            set_pump_relay(false);
            set_chlorinator_relay(false);
            set_light_mode(LIGHT_OFF);

            // Only process HMI/web commands (for reset) while stopped
            hmi_send_data();
            hmi_process_commands();
            web_server_send_realtime_data();
            wifi_manager_maintenance_task();

            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }

        // Reset daily acid tracking at midnight
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        uint32_t current_day = (uint32_t)timeinfo.tm_yday;
        if (current_day != g_state.daily_acid_reset_day) {
            g_state.daily_acid_dosed_ml = 0.0f;
            g_state.daily_acid_reset_day = current_day;
        }

        // Read sensors (only if pump has been running for 2+ minutes)
        if (g_state.pump_healthy &&
            (esp_timer_get_time() / 1000 - g_state.pump_start_time) >= SENSOR_STABILIZATION_TIME_MS) {
            g_state.sensors_healthy = true;

            float temp = atlas_read_temperature();
            float ph = atlas_read_ph();
            float orp = atlas_read_orp();

            // Only update state with valid (non-NAN) readings
            if (!isnan(temp)) g_state.temperature = temp;
            if (!isnan(ph)) g_state.ph = ph;
            if (!isnan(orp)) g_state.orp = orp;

            // sensors_healthy may have been set to false by atlas_read_* on failure
        } else {
            g_state.sensors_healthy = false;
        }

        // Read Shelly cached state (non-blocking)
        {
            shelly_channel_status_t pump_sh, chlorinator_sh;
            bool shelly_ok;
            shelly_get_cached_state(&pump_sh, &chlorinator_sh, &shelly_ok);

            g_state.shelly_reachable = shelly_ok;

            if (pump_sh.valid) {
                g_state.pump_current = pump_sh.current;
                g_state.pump_power_watts = pump_sh.apower;
                g_state.pump_voltage = pump_sh.voltage;
                g_state.pump_relay_on = pump_sh.output;
            }

            if (chlorinator_sh.valid) {
                g_state.chlorinator_current = chlorinator_sh.current;
                g_state.chlorinator_power_watts = chlorinator_sh.apower;
                g_state.chlorinator_voltage = chlorinator_sh.voltage;
                g_state.chlorinator_relay_on = chlorinator_sh.output;
            }

            g_state.shelly_temperature = pump_sh.temperature;

            // Shelly unreachable alarm management
            if (!shelly_ok && !g_state.shelly_alarm_active) {
                raise_alarm("SHELLY_UNREACHABLE", "Shelly 2PM Pro not responding");
                g_state.shelly_alarm_active = true;
            } else if (shelly_ok && g_state.shelly_alarm_active) {
                clear_alarm("SHELLY_UNREACHABLE");
                g_state.shelly_alarm_active = false;
            }
        }

        // Update equipment status
        update_pump_status();
        manage_duty_cycle();
        check_light_mode_change();

        // Chemical dosing (only when sensors AND pump are healthy)
        if (g_state.sensors_healthy && g_state.pump_healthy) {
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

void emergency_stop_activate(void)
{
    g_state.emergency_stop_active = true;
    atlas_stop_dosing();
    set_pump_relay(false);
    set_chlorinator_relay(false);
    set_light_mode(LIGHT_OFF);
    ESP_LOGE(TAG, "EMERGENCY STOP ACTIVATED");
    log_event("Emergency stop activated");
}

void emergency_stop_reset(void)
{
    g_state.emergency_stop_active = false;
    ESP_LOGW(TAG, "Emergency stop reset - resuming normal operation");
    log_event("Emergency stop reset");
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
