#include "pool_controller.h"

static const char *TAG = "CHEMICAL_DOSING";

void chemical_dosing_init(void)
{
    ESP_LOGI(TAG, "Initializing Chemical Dosing System...");
    
    // Initialize timing variables
    g_state.last_acid_addition_time = 0;
    g_state.last_ph_correction_time = 0;
    
    ESP_LOGI(TAG, "Chemical Dosing System Initialized");
}

void perform_base_acid_addition(void)
{
    // Safety interlock: require healthy pump for water circulation
    if (!g_state.pump_healthy) {
        return;
    }

    // Only perform base acid addition if enough time has passed
    uint64_t current_time = get_timestamp_ms();

    if (g_state.last_acid_addition_time == 0) {
        g_state.last_acid_addition_time = current_time;
        return;
    }

    uint64_t time_since_last_addition = current_time - g_state.last_acid_addition_time;

    if (time_since_last_addition < g_config.acid_addition_interval_ms) {
        return; // Not time yet
    }

    // Calculate acid amount based on chlorinator runtime
    float chlorinator_runtime_hours_since_last =
        (float)(time_since_last_addition) / (1000.0f * 3600.0f);

    // Only add acid if chlorinator has been running
    if (g_state.chlorinator_runtime_hours > 0) {
        float acid_amount = g_config.base_acid_rate * chlorinator_runtime_hours_since_last;

        // Apply learning adjustment
        float learning_adjustment = (g_config.base_acid_learning_gain / 100.0f);
        acid_amount *= (1.0f + learning_adjustment);

        // Safety limits
        if (acid_amount < MIN_BASE_ACID_DOSE_ML || acid_amount > MAX_BASE_ACID_DOSE_ML) {
            return;
        }

        // Check daily limit
        if (g_state.daily_acid_dosed_ml + acid_amount > MAX_DAILY_ACID_ML) {
            ESP_LOGW(TAG, "Daily acid limit reached: %.1fml dosed today, %.1fml requested",
                     g_state.daily_acid_dosed_ml, acid_amount);
            raise_alarm("ACID_DAILY_LIMIT", "Daily acid dosing limit reached");
            return;
        }

        // Check if we have enough acid
        if (g_state.acid_remaining_ml >= acid_amount) {

            esp_err_t result = atlas_dose_acid(acid_amount);

            if (result == ESP_OK) {
                g_state.acid_remaining_ml -= acid_amount;
                g_state.daily_acid_dosed_ml += acid_amount;
                g_state.last_acid_addition_time = current_time;

                system_config_save_runtime_state();

                ESP_LOGI(TAG, "Base acid addition: %.1fml (Runtime: %.1fh, Rate: %.1fml/h)",
                         acid_amount, chlorinator_runtime_hours_since_last, g_config.base_acid_rate);

                char log_msg[128];
                snprintf(log_msg, sizeof(log_msg),
                         "Base acid: %.1fml added (%.1fml remaining, %.1fml today)",
                         acid_amount, g_state.acid_remaining_ml, g_state.daily_acid_dosed_ml);
                log_event(log_msg);
            } else {
                ESP_LOGE(TAG, "Failed to dose acid: %s", esp_err_to_name(result));
            }
        } else {
            ESP_LOGW(TAG, "Insufficient acid for base addition: %.1fml needed, %.1fml available",
                     acid_amount, g_state.acid_remaining_ml);
        }
    }
}

void perform_ph_correction(void)
{
    // Safety interlocks
    if (!g_state.sensors_healthy || !g_state.pump_healthy) {
        return;
    }

    float ph_error = g_state.ph - g_config.ph_target;

    // Only correct if pH is significantly high (> 0.1 above target)
    if (ph_error <= 0.1f) {
        return;
    }

    uint64_t current_time = get_timestamp_ms();

    // Wait at least 10 minutes after pump start before first correction
    if ((current_time - g_state.pump_start_time) < PH_CORRECTION_PUMP_DELAY_MS) {
        return;
    }

    // Wait at least 30 minutes between corrections
    if (g_state.last_ph_correction_time > 0 &&
        (current_time - g_state.last_ph_correction_time) < PH_CORRECTION_INTERVAL_MS) {
        return;
    }

    // Calculate correction amount using learning system
    float base_correction = g_config.ph_correction_base_amount;
    float learning_factor = g_state.last_ph_correction_perc / 100.0f;
    float correction_amount = base_correction * learning_factor;

    // Apply pH error scaling
    float error_scaling = ph_error / PH_ERROR_SCALING_REFERENCE;
    if (error_scaling > PH_ERROR_SCALING_MAX) error_scaling = PH_ERROR_SCALING_MAX;
    correction_amount *= error_scaling;

    // Safety limits
    if (correction_amount < MIN_PH_CORRECTION_ML) correction_amount = MIN_PH_CORRECTION_ML;
    if (correction_amount > MAX_PH_CORRECTION_ML) correction_amount = MAX_PH_CORRECTION_ML;

    // Check daily limit
    if (g_state.daily_acid_dosed_ml + correction_amount > MAX_DAILY_ACID_ML) {
        ESP_LOGW(TAG, "Daily acid limit reached: %.1fml dosed today, %.1fml requested",
                 g_state.daily_acid_dosed_ml, correction_amount);
        raise_alarm("ACID_DAILY_LIMIT", "Daily acid dosing limit reached");
        return;
    }

    // Check if we have enough acid
    if (g_state.acid_remaining_ml >= correction_amount) {

        esp_err_t result = atlas_dose_acid(correction_amount);

        if (result == ESP_OK) {
            g_state.acid_remaining_ml -= correction_amount;
            g_state.daily_acid_dosed_ml += correction_amount;
            g_state.last_ph_correction_time = current_time;

            // Store pH before correction for learning evaluation
            float ph_before = g_state.ph;

            // Schedule learning evaluation
            g_state.ph_learning_evaluation_pending = true;
            g_state.ph_learning_evaluation_time = current_time + PH_LEARNING_EVAL_DELAY_MS;

            // Store correction data for learning
            int history_index = g_state.ph_history_index;
            g_state.ph_history[history_index].ph_before = ph_before;
            g_state.ph_history[history_index].correction_amount = correction_amount;
            g_state.ph_history[history_index].expected_change = calculate_expected_ph_change(correction_amount);
            g_state.ph_history[history_index].timestamp = current_time;

            system_config_save_runtime_state();
            system_config_save_learning_history();

            ESP_LOGI(TAG, "pH correction: %.1fml dosed (pH: %.2f, Target: %.2f, Error: %.2f)",
                     correction_amount, ph_before, g_config.ph_target, ph_error);

            char log_msg[128];
            snprintf(log_msg, sizeof(log_msg),
                     "pH correction: %.1fml (pH %.2f→%.2f target, %.1fml remaining, %.1fml today)",
                     correction_amount, ph_before, g_config.ph_target,
                     g_state.acid_remaining_ml, g_state.daily_acid_dosed_ml);
            log_event(log_msg);

        } else {
            ESP_LOGE(TAG, "Failed to dose acid for pH correction: %s", esp_err_to_name(result));
        }
    } else {
        ESP_LOGW(TAG, "Insufficient acid for pH correction: %.1fml needed, %.1fml available",
                 correction_amount, g_state.acid_remaining_ml);
    }
}

void track_chlorinator_runtime(void)
{
    static uint64_t last_runtime_update = 0;
    uint64_t current_time = get_timestamp_ms();
    
    if (last_runtime_update == 0) {
        last_runtime_update = current_time;
        return;
    }
    
    // If chlorinator is currently running, add to runtime
    if (g_state.chlorinator_relay_on) {
        uint64_t time_diff = current_time - last_runtime_update;
        float hours_diff = (float)time_diff / (1000.0f * 3600.0f); // Convert ms to hours
        
        g_state.chlorinator_runtime_hours += hours_diff;
        
        // Save runtime to NVS every hour
        static uint32_t save_counter = 0;
        if (++save_counter >= 3600) { // Every hour (assuming 1-second task cycle)
            save_counter = 0;
            system_config_save_runtime_state();
        }
    }
    
    last_runtime_update = current_time;
}

float calculate_expected_ph_change(float acid_ml)
{
    // Calculate expected pH change using proper acid-base chemistry.
    //
    // HCl density ~1.16 g/mL at 32.5%. Molecular weight of HCl = 36.46 g/mol.
    // acid_ml of solution at X% concentration → moles of H+ added to pool.

    float hcl_density = 1.16f; // g/mL for ~32% HCl
    float hcl_mw = 36.46f;    // g/mol

    // Moles of HCl = (concentration% / 100) * volume_mL * density_g/mL / molecular_weight
    float acid_moles = (g_config.hcl_concentration_percent / 100.0f) *
                       acid_ml * hcl_density / hcl_mw;

    // Current H+ concentration from existing pH
    float initial_h = powf(10.0f, -g_state.ph);

    // Additional H+ concentration in pool
    float h_added = acid_moles / g_config.pool_volume_liters;

    // New total H+ concentration
    float final_h = initial_h + h_added;

    // Expected pH change (negative = pH decreases = more acidic)
    float new_ph = -log10f(final_h);
    float expected_ph_change = new_ph - g_state.ph;

    // Apply buffering factor - pool alkalinity resists pH change
    // Typical pool (80-120 ppm alkalinity) buffers ~70% of theoretical change
    expected_ph_change *= 0.3f;

    // Safety limits
    if (expected_ph_change < -2.0f) expected_ph_change = -2.0f;
    if (expected_ph_change > 0.0f) expected_ph_change = 0.0f;

    return expected_ph_change;
}

void refill_acid_bottle(float new_volume_ml)
{
    g_state.acid_remaining_ml = new_volume_ml;
    
    // Clear low acid alarm
    if (g_state.acid_low_alarm_active) {
        clear_alarm("ACID_LOW");
        g_state.acid_low_alarm_active = false;
    }
    
    // Save to NVS
    system_config_save_runtime_state();
    
    ESP_LOGI(TAG, "Acid bottle refilled: %.1fml", new_volume_ml);
    
    char log_msg[64];
    snprintf(log_msg, sizeof(log_msg), "Acid bottle refilled: %.1fml", new_volume_ml);
    log_event(log_msg);
}

void manual_acid_dose(float volume_ml)
{
    // Manual acid dosing with safety checks
    if (volume_ml <= 0.0f || volume_ml > 500.0f) {
        ESP_LOGW(TAG, "Invalid manual dose amount: %.1fml", volume_ml);
        return;
    }
    
    if (g_state.acid_remaining_ml < volume_ml) {
        ESP_LOGW(TAG, "Insufficient acid for manual dose: %.1fml requested, %.1fml available", 
                 volume_ml, g_state.acid_remaining_ml);
        return;
    }
    
    if (!g_state.pump_healthy) {
        ESP_LOGW(TAG, "Cannot dose acid - pump not healthy");
        return;
    }
    
    esp_err_t result = atlas_dose_acid(volume_ml);
    
    if (result == ESP_OK) {
        g_state.acid_remaining_ml -= volume_ml;
        
        // Save updated acid level
        system_config_save_runtime_state();
        
        ESP_LOGI(TAG, "Manual acid dose: %.1fml (%.1fml remaining)", 
                 volume_ml, g_state.acid_remaining_ml);
        
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "Manual acid dose: %.1fml", volume_ml);
        log_event(log_msg);
        
    } else {
        ESP_LOGE(TAG, "Failed manual acid dose: %s", esp_err_to_name(result));
    }
}

float get_daily_acid_consumption(void)
{
    // Calculate daily acid consumption based on current rate
    float daily_chlorinator_hours = 24.0f * (g_config.chlorinator_duty_cycle / 100.0f);
    float daily_base_acid = daily_chlorinator_hours * g_config.base_acid_rate;
    
    // Add estimated pH corrections (assume 2 per day)
    float daily_corrections = 2.0f * g_config.ph_correction_base_amount;
    
    return daily_base_acid + daily_corrections;
}

uint32_t get_acid_days_remaining(void)
{
    float daily_consumption = get_daily_acid_consumption();
    
    if (daily_consumption <= 0.0f) {
        return 999; // Effectively unlimited
    }
    
    return (uint32_t)(g_state.acid_remaining_ml / daily_consumption);
}
