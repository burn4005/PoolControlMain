#include "pool_controller.h"

static const char *TAG = "CHLORINATOR_CONTROL";

static uint64_t chlorinator_alarm_timer = 0;

void chlorinator_control_init(void)
{
    ESP_LOGI(TAG, "Initializing Chlorinator Control (Shelly)...");

    g_state.chlorinator_relay_on = false;
    g_state.duty_cycle_active = false;
    g_state.duty_cycle_start_time = get_timestamp_ms();
    
    // Calculate duty cycle timing
    g_state.duty_on_time_ms = (g_config.duty_cycle_period_ms * g_config.chlorinator_duty_cycle) / 100.0f;
    
    ESP_LOGI(TAG, "Chlorinator Control Initialized (Shelly mode) - Duty: %.1f%%, Period: %lums",
             g_config.chlorinator_duty_cycle, g_config.duty_cycle_period_ms);
}

void manage_duty_cycle(void)
{
    if (!chlorinator_interlock_ok()) {
        // Interlock failed - turn off chlorinator
        if (g_state.chlorinator_relay_on || g_state.duty_cycle_active) {
            set_chlorinator_relay(false);
            g_state.duty_cycle_active = false;
            ESP_LOGW(TAG, "Chlorinator stopped - interlock failed: %s", get_interlock_failure_reason());
        }
        return;
    }
    
    // Interlock OK - manage duty cycle
    uint64_t current_time = get_timestamp_ms();
    uint64_t cycle_elapsed = current_time - g_state.duty_cycle_start_time;
    
    if (cycle_elapsed >= g_config.duty_cycle_period_ms) {
        // Start new cycle
        g_state.duty_cycle_start_time = current_time;
        cycle_elapsed = 0;
    }
    
    if (cycle_elapsed < g_state.duty_on_time_ms) {
        // ON period
        if (!g_state.duty_cycle_active) {
            set_chlorinator_relay(true);
            g_state.duty_cycle_active = true;
            ESP_LOGI(TAG, "Chlorinator duty cycle ON - %.1f%% duty", g_config.chlorinator_duty_cycle);
        }
    } else {
        // OFF period
        if (g_state.duty_cycle_active) {
            set_chlorinator_relay(false);
            g_state.duty_cycle_active = false;
            ESP_LOGI(TAG, "Chlorinator duty cycle OFF");
        }
    }
    
    // Monitor chlorinator current for alarms
    if (g_state.chlorinator_relay_on) {
        if (g_state.chlorinator_current < g_config.chlorinator_min_current) {
            if (chlorinator_alarm_timer == 0) {
                chlorinator_alarm_timer = current_time;
            } else if ((current_time - chlorinator_alarm_timer) > 60000 && !g_state.chlorinator_alarm_active) {
                raise_alarm("CHLORINATOR_LOW_CURRENT", "Low salt level detected - Add salt to pool");
                g_state.chlorinator_alarm_active = true;
            }
        } else if (g_state.chlorinator_current > g_config.chlorinator_max_current) {
            raise_alarm("CHLORINATOR_HIGH_CURRENT", "Chlorinator overload - Check cell condition");
            g_state.chlorinator_alarm_active = true;
        } else {
            chlorinator_alarm_timer = 0;
            if (g_state.chlorinator_alarm_active) {
                clear_alarm("CHLORINATOR_CURRENT");
                g_state.chlorinator_alarm_active = false;
            }
        }
    } else {
        chlorinator_alarm_timer = 0;
    }
}

bool chlorinator_interlock_ok(void)
{
    // Primary requirement: Pump must be healthy
    if (!g_state.pump_healthy) {
        return false;
    }
    
    // Pump must be running (not OFF)
    if (g_state.pump_status == PUMP_OFF) {
        return false;
    }
    
    // Pump must have been healthy for minimum time
    uint64_t current_time = get_timestamp_ms();
    if ((current_time - g_state.pump_start_time) < MIN_PUMP_RUNTIME_MS) {
        return false;
    }
    
    // No pump alarms active
    if (g_state.pump_alarm_active) {
        return false;
    }
    
    return true;
}

const char* get_interlock_failure_reason(void)
{
    static char reason[128];
    
    if (!g_state.pump_healthy) {
        snprintf(reason, sizeof(reason), "Pump unhealthy - %s", get_pump_status_string(g_state.pump_status));
        return reason;
    }
    
    if (g_state.pump_status == PUMP_OFF) {
        return "Pump is OFF - Start pump first";
    }
    
    uint64_t current_time = get_timestamp_ms();
    if ((current_time - g_state.pump_start_time) < MIN_PUMP_RUNTIME_MS) {
        uint64_t remaining_ms = MIN_PUMP_RUNTIME_MS - (current_time - g_state.pump_start_time);
        snprintf(reason, sizeof(reason), "Pump stabilizing - Wait %llu seconds", remaining_ms / 1000);
        return reason;
    }
    
    if (g_state.pump_alarm_active) {
        return "Pump alarm active - Clear faults first";
    }
    
    return "Unknown interlock failure";
}

void set_chlorinator_relay(bool state)
{
    shelly_queue_switch(SHELLY_CH_CHLORINATOR, state);
    g_state.chlorinator_relay_on = state;
    
    if (state) {
        g_state.chlorinator_start_time = get_timestamp_ms();
        ESP_LOGI(TAG, "Chlorinator relay turned ON");
    } else {
        ESP_LOGI(TAG, "Chlorinator relay turned OFF");
    }
}

void update_duty_cycle_settings(float duty_percent, uint32_t period_ms)
{
    g_config.chlorinator_duty_cycle = duty_percent;
    g_config.duty_cycle_period_ms = period_ms;
    
    // Recalculate duty timing
    g_state.duty_on_time_ms = (period_ms * duty_percent) / 100.0f;
    
    // Reset duty cycle timing
    g_state.duty_cycle_start_time = get_timestamp_ms();
    
    ESP_LOGI(TAG, "Duty cycle updated - %.1f%% duty, %lums period", duty_percent, period_ms);
    log_event("Chlorinator duty cycle settings updated");
}
