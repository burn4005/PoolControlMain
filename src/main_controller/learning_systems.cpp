#include "pool_controller.h"

static const char *TAG = "LEARNING_SYSTEMS";

void learning_systems_init(void)
{
    ESP_LOGI(TAG, "Initializing Learning Systems...");
    
    // Initialize learning history indices
    g_state.ph_history_index = 0;
    g_state.orp_history_index = 0;
    
    // Initialize learning evaluation flags
    g_state.ph_learning_evaluation_pending = false;
    g_state.orp_learning_evaluation_pending = false;
    
    ESP_LOGI(TAG, "Learning Systems Initialized");
}

void evaluate_ph_correction_effectiveness(void)
{
    if (!g_state.ph_learning_evaluation_pending) {
        return;
    }
    
    uint64_t current_time = get_timestamp_ms();
    
    // Check if enough time has passed for evaluation (2 hours)
    if (current_time < g_state.ph_learning_evaluation_time) {
        return;
    }
    
    // Clear the pending flag
    g_state.ph_learning_evaluation_pending = false;
    
    // Get the current pH correction history entry
    int history_index = g_state.ph_history_index;
    ph_correction_history_t *correction = &g_state.ph_history[history_index];
    
    // Store the pH after correction
    correction->ph_after = g_state.ph;
    correction->actual_change = correction->ph_after - correction->ph_before;
    
    // Calculate effectiveness ratio
    if (correction->expected_change != 0.0f) {
        correction->effectiveness_ratio = correction->actual_change / correction->expected_change;
    } else {
        correction->effectiveness_ratio = 1.0f; // Default if no expected change
    }
    
    // Clamp effectiveness ratio to reasonable bounds
    if (correction->effectiveness_ratio < 0.1f) correction->effectiveness_ratio = 0.1f;
    if (correction->effectiveness_ratio > 3.0f) correction->effectiveness_ratio = 3.0f;
    
    // Update learning gain based on effectiveness
    update_ph_learning_gain(correction->effectiveness_ratio);
    
    // Move to next history index (circular buffer)
    g_state.ph_history_index = (g_state.ph_history_index + 1) % 10;
    
    // Save learning history to NVS
    system_config_save_learning_history();
    
    ESP_LOGI(TAG, "pH correction evaluated: Expected %.3f, Actual %.3f, Ratio %.2f, New gain %.1f%%",
             correction->expected_change, correction->actual_change, 
             correction->effectiveness_ratio, g_config.ph_correction_learning_gain);
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), 
             "pH learning: %.1f%% effectiveness, gain now %.1f%%", 
             correction->effectiveness_ratio * 100.0f, g_config.ph_correction_learning_gain);
    log_event(log_msg);
}

void update_ph_learning_gain(float effectiveness_ratio)
{
    // Adaptive learning gain adjustment
    float current_gain = g_config.ph_correction_learning_gain;
    float adjustment_rate = g_config.learning_rate / 100.0f; // Convert percentage to decimal
    
    if (effectiveness_ratio < 0.8f) {
        // Correction was less effective than expected - increase gain
        float increase = (0.8f - effectiveness_ratio) * adjustment_rate * 100.0f;
        current_gain += increase;
    } else if (effectiveness_ratio > 1.2f) {
        // Correction was more effective than expected - decrease gain
        float decrease = (effectiveness_ratio - 1.2f) * adjustment_rate * 100.0f;
        current_gain -= decrease;
    }
    
    // Apply limits to learning gain
    if (current_gain < 5.0f) current_gain = 5.0f;     // Minimum 5%
    if (current_gain > 50.0f) current_gain = 50.0f;   // Maximum 50%
    
    g_config.ph_correction_learning_gain = current_gain;
    
    // Update last correction percentage for next correction
    g_state.last_ph_correction_perc = current_gain;
    
    // Save updated configuration
    system_config_save();
}

void evaluate_orp_adjustment_effectiveness(void)
{
    if (!g_state.orp_learning_evaluation_pending) {
        return;
    }
    
    uint64_t current_time = get_timestamp_ms();
    
    // Check if enough time has passed for evaluation (30 minutes)
    if (current_time < g_state.orp_learning_evaluation_time) {
        return;
    }
    
    // Clear the pending flag
    g_state.orp_learning_evaluation_pending = false;
    
    // Get the current ORP adjustment history entry
    int history_index = g_state.orp_history_index;
    orp_adjustment_history_t *adjustment = &g_state.orp_history[history_index];
    
    // Store the ORP after adjustment
    adjustment->orp_after = g_state.orp;
    adjustment->actual_orp_change = adjustment->orp_after - adjustment->orp_before;
    
    // Calculate effectiveness ratio
    if (adjustment->expected_orp_change != 0.0f) {
        adjustment->effectiveness_ratio = adjustment->actual_orp_change / adjustment->expected_orp_change;
    } else {
        adjustment->effectiveness_ratio = 1.0f;
    }
    
    // Clamp effectiveness ratio to reasonable bounds
    if (adjustment->effectiveness_ratio < 0.1f) adjustment->effectiveness_ratio = 0.1f;
    if (adjustment->effectiveness_ratio > 3.0f) adjustment->effectiveness_ratio = 3.0f;
    
    // Update learning gain based on effectiveness
    update_orp_learning_gain(adjustment->effectiveness_ratio);
    
    // Move to next history index (circular buffer)
    g_state.orp_history_index = (g_state.orp_history_index + 1) % 10;
    
    // Save learning history to NVS
    system_config_save_learning_history();
    
    ESP_LOGI(TAG, "ORP adjustment evaluated: Expected %.1fmV, Actual %.1fmV, Ratio %.2f, New gain %.1f%%",
             adjustment->expected_orp_change, adjustment->actual_orp_change, 
             adjustment->effectiveness_ratio, g_config.orp_learning_gain);
}

void update_orp_learning_gain(float effectiveness_ratio)
{
    // Adaptive learning gain adjustment for ORP
    float current_gain = g_config.orp_learning_gain;
    float adjustment_rate = g_config.learning_rate / 100.0f;
    
    if (effectiveness_ratio < 0.8f) {
        // Adjustment was less effective than expected - increase gain
        float increase = (0.8f - effectiveness_ratio) * adjustment_rate * 100.0f;
        current_gain += increase;
    } else if (effectiveness_ratio > 1.2f) {
        // Adjustment was more effective than expected - decrease gain
        float decrease = (effectiveness_ratio - 1.2f) * adjustment_rate * 100.0f;
        current_gain -= decrease;
    }
    
    // Apply limits to ORP learning gain
    if (current_gain < 5.0f) current_gain = 5.0f;     // Minimum 5%
    if (current_gain > 30.0f) current_gain = 30.0f;   // Maximum 30%
    
    g_config.orp_learning_gain = current_gain;
    
    // Save updated configuration
    system_config_save();
}

void adjust_chlorinator_for_orp(void)
{
    static uint64_t last_orp_adjustment = 0;
    uint64_t current_time = get_timestamp_ms();
    
    // Only adjust every 30 minutes (or configured interval)
    if (last_orp_adjustment > 0 && 
        (current_time - last_orp_adjustment) < g_config.orp_adjustment_interval_ms) {
        return;
    }
    
    // Calculate optimal ORP target (temperature and time compensated)
    float target_orp = calculate_optimal_orp_target();
    float orp_error = g_state.orp - target_orp;
    
    // Only adjust if ORP error is significant (> 25mV)
    if (fabsf(orp_error) < 25.0f) {
        return;
    }
    
    // Calculate duty cycle adjustment based on ORP error
    float duty_adjustment = 0.0f;
    
    if (orp_error < -25.0f) {
        // ORP too low - increase chlorinator duty cycle
        duty_adjustment = (-orp_error / 50.0f) * g_config.orp_learning_gain / 100.0f;
    } else if (orp_error > 25.0f) {
        // ORP too high - decrease chlorinator duty cycle
        duty_adjustment = (-orp_error / 50.0f) * g_config.orp_learning_gain / 100.0f;
    }
    
    // Apply limits to duty cycle adjustment
    if (duty_adjustment > 10.0f) duty_adjustment = 10.0f;   // Max +10%
    if (duty_adjustment < -10.0f) duty_adjustment = -10.0f; // Max -10%
    
    // Calculate new duty cycle
    float new_duty_cycle = g_config.chlorinator_duty_cycle + duty_adjustment;
    
    // Apply absolute limits to duty cycle
    if (new_duty_cycle < 0.0f) new_duty_cycle = 0.0f;
    if (new_duty_cycle > 100.0f) new_duty_cycle = 100.0f;
    
    // Only apply adjustment if it's significant (> 1%)
    if (fabsf(duty_adjustment) >= 1.0f) {
        
        // Store ORP adjustment data for learning evaluation
        int history_index = g_state.orp_history_index;
        g_state.orp_history[history_index].orp_before = g_state.orp;
        g_state.orp_history[history_index].duty_adjustment = duty_adjustment;
        g_state.orp_history[history_index].expected_orp_change = -orp_error * 0.7f; // Expect 70% correction
        g_state.orp_history[history_index].timestamp = current_time;
        
        // Schedule learning evaluation in 30 minutes
        g_state.orp_learning_evaluation_pending = true;
        g_state.orp_learning_evaluation_time = current_time + 1800000; // 30 minutes
        
        // Update chlorinator duty cycle
        system_config_update_chlorinator_settings(new_duty_cycle, g_config.duty_cycle_period_ms);
        
        last_orp_adjustment = current_time;
        
        ESP_LOGI(TAG, "ORP adjustment: %.1fmV error, %.1f%% duty change (%.1f%% → %.1f%%)",
                 orp_error, duty_adjustment, g_config.chlorinator_duty_cycle - duty_adjustment, new_duty_cycle);
        
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), 
                 "ORP learning: %.0fmV error, duty %.1f%% → %.1f%%", 
                 orp_error, g_config.chlorinator_duty_cycle - duty_adjustment, new_duty_cycle);
        log_event(log_msg);
    }
}

void update_base_acid_learning(void)
{
    // Analyze pH trends over the last 24 hours to adjust base acid rate
    static uint64_t last_base_acid_learning = 0;
    uint64_t current_time = get_timestamp_ms();
    
    // Only evaluate once per day
    if (last_base_acid_learning > 0 && 
        (current_time - last_base_acid_learning) < 86400000) { // 24 hours
        return;
    }
    
    // Calculate average pH over recent history
    float ph_sum = 0.0f;
    int valid_readings = 0;
    
    for (int i = 0; i < 10; i++) {
        if (g_state.ph_history[i].timestamp > 0 && 
            (current_time - g_state.ph_history[i].timestamp) < 86400000) { // Last 24 hours
            ph_sum += g_state.ph_history[i].ph_before;
            valid_readings++;
        }
    }
    
    if (valid_readings >= 3) { // Need at least 3 readings
        float average_ph = ph_sum / valid_readings;
        float ph_trend = average_ph - g_config.ph_target;
        
        // Adjust base acid rate based on pH trend
        float rate_adjustment = 0.0f;
        
        if (ph_trend > 0.1f) {
            // pH trending high - increase base acid rate
            rate_adjustment = ph_trend * g_config.base_acid_learning_gain / 100.0f;
        } else if (ph_trend < -0.1f) {
            // pH trending low - decrease base acid rate
            rate_adjustment = ph_trend * g_config.base_acid_learning_gain / 100.0f;
        }
        
        // Apply adjustment
        float new_rate = g_config.base_acid_rate + rate_adjustment;
        
        // Apply limits
        if (new_rate < 2.0f) new_rate = 2.0f;     // Minimum 2ml/h
        if (new_rate > 50.0f) new_rate = 50.0f;   // Maximum 50ml/h
        
        if (fabsf(rate_adjustment) >= 0.5f) { // Only apply significant changes
            g_config.base_acid_rate = new_rate;
            system_config_save();
            
            ESP_LOGI(TAG, "Base acid learning: pH trend %.2f, rate %.1f → %.1fml/h",
                     ph_trend, g_config.base_acid_rate - rate_adjustment, new_rate);
        }
    }
    
    last_base_acid_learning = current_time;
}

float get_learning_effectiveness_ph(void)
{
    // Calculate average pH correction effectiveness over recent history
    float effectiveness_sum = 0.0f;
    int valid_corrections = 0;
    
    for (int i = 0; i < 10; i++) {
        if (g_state.ph_history[i].timestamp > 0 && g_state.ph_history[i].effectiveness_ratio > 0.0f) {
            effectiveness_sum += g_state.ph_history[i].effectiveness_ratio;
            valid_corrections++;
        }
    }
    
    if (valid_corrections > 0) {
        return (effectiveness_sum / valid_corrections) * 100.0f; // Convert to percentage
    }
    
    return 100.0f; // Default if no history
}

float get_learning_effectiveness_orp(void)
{
    // Calculate average ORP adjustment effectiveness over recent history
    float effectiveness_sum = 0.0f;
    int valid_adjustments = 0;
    
    for (int i = 0; i < 10; i++) {
        if (g_state.orp_history[i].timestamp > 0 && g_state.orp_history[i].effectiveness_ratio > 0.0f) {
            effectiveness_sum += g_state.orp_history[i].effectiveness_ratio;
            valid_adjustments++;
        }
    }
    
    if (valid_adjustments > 0) {
        return (effectiveness_sum / valid_adjustments) * 100.0f; // Convert to percentage
    }
    
    return 100.0f; // Default if no history
}
