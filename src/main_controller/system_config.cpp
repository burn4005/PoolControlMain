#include "pool_controller.h"

static const char *TAG = "SYSTEM_CONFIG";
static const char *NVS_NAMESPACE = "pool_config";

esp_err_t system_config_init(void)
{
    ESP_LOGI(TAG, "Initializing System Configuration...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "System Configuration Initialized");
    return ESP_OK;
}

void system_config_set_defaults(void)
{
    ESP_LOGI(TAG, "Setting default configuration...");
    
    // Pool Configuration
    g_config.pool_volume_liters = POOL_VOLUME_LITERS;
    g_config.hcl_concentration_percent = HCL_CONCENTRATION_PERCENT;
    g_config.acid_bottle_size_ml = ACID_BOTTLE_SIZE_ML;
    g_config.acid_low_alert_ml = ACID_LOW_ALERT_ML;
    
    // pH Control Setpoints
    g_config.ph_target = 7.6f;
    g_config.ph_correction_base_amount = 50.0f;
    g_config.ph_correction_learning_gain = 10.0f;
    g_config.base_acid_rate = 10.0f;
    g_config.base_acid_learning_gain = 20.0f;
    g_config.learning_rate = 10.0f;
    
    // ORP Control Setpoints
    g_config.base_orp_target = 700.0f;
    g_config.orp_temp_coefficient = -2.0f;
    g_config.orp_learning_gain = 10.0f;
    g_config.orp_adjustment_interval_ms = 1800000; // 30 minutes
    
    // Chlorinator Control Setpoints
    g_config.chlorinator_duty_cycle = 25.0f;
    g_config.duty_cycle_period_ms = 600000; // 10 minutes
    g_config.acid_addition_interval_ms = 3600000; // 60 minutes
    
    // Pump Current Thresholds (User Configurable)
    g_config.pump_thresholds.stopped_max = 0.5f;
    g_config.pump_thresholds.low_speed_min = 2.0f;
    g_config.pump_thresholds.medium_speed_min = 4.0f;
    g_config.pump_thresholds.high_speed_min = 6.0f;
    
    // Current Monitoring Setpoints
    g_config.chlorinator_min_current = 0.3f;
    g_config.chlorinator_max_current = 0.8f;
    
    // Time Settings
    g_config.timezone_offset_hours = 10; // Brisbane/Australia
    strcpy(g_config.ntp_server_primary, "pool.ntp.org");
    strcpy(g_config.ntp_server_backup, "time.nist.gov");
    
    // WiFi Settings (empty by default)
    strcpy(g_config.wifi_ssid, "");
    strcpy(g_config.wifi_password, "");
    
    ESP_LOGI(TAG, "Default configuration set - All setpoints initialized");
}

esp_err_t system_config_save(void)
{
    ESP_LOGI(TAG, "Saving configuration to NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save entire configuration structure
    ret = nvs_set_blob(nvs_handle, "config", &g_config, sizeof(system_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit configuration: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved successfully");
    }
    
    return ret;
}

esp_err_t system_config_load(void)
{
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle, using defaults: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Load configuration structure
    size_t required_size = sizeof(system_config_t);
    ret = nvs_get_blob(nvs_handle, "config", &g_config, &required_size);
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded successfully");
        
        // Validate loaded configuration
        if (g_config.pool_volume_liters <= 0 || g_config.pool_volume_liters > 100000) {
            ESP_LOGW(TAG, "Invalid pool volume, using default");
            g_config.pool_volume_liters = POOL_VOLUME_LITERS;
        }
        
        if (g_config.ph_target < 6.0f || g_config.ph_target > 9.0f) {
            ESP_LOGW(TAG, "Invalid pH target, using default");
            g_config.ph_target = 7.6f;
        }
        
        if (g_config.chlorinator_duty_cycle < 0.0f || g_config.chlorinator_duty_cycle > 100.0f) {
            ESP_LOGW(TAG, "Invalid duty cycle, using default");
            g_config.chlorinator_duty_cycle = 25.0f;
        }
        
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved configuration found, using defaults");
        ret = ESP_OK; // Not an error, just use defaults
    } else {
        ESP_LOGE(TAG, "Failed to load configuration: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t system_config_reset_to_defaults(void)
{
    ESP_LOGI(TAG, "Resetting configuration to defaults...");
    
    system_config_set_defaults();
    
    esp_err_t ret = system_config_save();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration reset to defaults and saved");
    }
    
    return ret;
}

esp_err_t system_config_update_pump_thresholds(float stopped_max, float low_min, float medium_min, float high_min)
{
    g_config.pump_thresholds.stopped_max = stopped_max;
    g_config.pump_thresholds.low_speed_min = low_min;
    g_config.pump_thresholds.medium_speed_min = medium_min;
    g_config.pump_thresholds.high_speed_min = high_min;
    
    ESP_LOGI(TAG, "Pump thresholds updated: stopped=%.1fA, low=%.1fA, medium=%.1fA, high=%.1fA",
             stopped_max, low_min, medium_min, high_min);
    
    return system_config_save();
}

esp_err_t system_config_update_ph_settings(float target, float base_amount, float learning_gain)
{
    g_config.ph_target = target;
    g_config.ph_correction_base_amount = base_amount;
    g_config.ph_correction_learning_gain = learning_gain;
    
    ESP_LOGI(TAG, "pH settings updated: target=%.2f, base=%.1fml, gain=%.1f%%",
             target, base_amount, learning_gain);
    
    return system_config_save();
}

esp_err_t system_config_update_orp_settings(float target, float temp_coeff, float learning_gain, uint32_t interval_ms)
{
    g_config.base_orp_target = target;
    g_config.orp_temp_coefficient = temp_coeff;
    g_config.orp_learning_gain = learning_gain;
    g_config.orp_adjustment_interval_ms = interval_ms;
    
    ESP_LOGI(TAG, "ORP settings updated: target=%.0fmV, temp_coeff=%.1f, gain=%.1f%%, interval=%lums",
             target, temp_coeff, learning_gain, interval_ms);
    
    return system_config_save();
}

esp_err_t system_config_update_chlorinator_settings(float duty_cycle, uint32_t period_ms)
{
    g_config.chlorinator_duty_cycle = duty_cycle;
    g_config.duty_cycle_period_ms = period_ms;
    
    // Update runtime state
    g_state.duty_on_time_ms = (period_ms * duty_cycle) / 100.0f;
    g_state.duty_cycle_start_time = get_timestamp_ms();
    
    ESP_LOGI(TAG, "Chlorinator settings updated: duty=%.1f%%, period=%lums",
             duty_cycle, period_ms);
    
    return system_config_save();
}

esp_err_t system_config_update_wifi_settings(const char* ssid, const char* password)
{
    strncpy(g_config.wifi_ssid, ssid, sizeof(g_config.wifi_ssid) - 1);
    g_config.wifi_ssid[sizeof(g_config.wifi_ssid) - 1] = '\0';
    
    strncpy(g_config.wifi_password, password, sizeof(g_config.wifi_password) - 1);
    g_config.wifi_password[sizeof(g_config.wifi_password) - 1] = '\0';
    
    ESP_LOGI(TAG, "WiFi settings updated: SSID=%s", ssid);
    
    return system_config_save();
}

esp_err_t system_config_save_runtime_state(void)
{
    ESP_LOGI(TAG, "Saving runtime state to NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for runtime state: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save acid remaining level
    ret = nvs_set_blob(nvs_handle, "acid_remaining", &g_state.acid_remaining_ml, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save acid remaining: %s", esp_err_to_name(ret));
    }
    
    // Save chlorinator runtime hours
    ret = nvs_set_blob(nvs_handle, "chlorinator_runtime", &g_state.chlorinator_runtime_hours, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save chlorinator runtime: %s", esp_err_to_name(ret));
    }
    
    // Save learning gains (these adapt over time)
    ret = nvs_set_blob(nvs_handle, "ph_learning_gain", &g_config.ph_correction_learning_gain, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save pH learning gain: %s", esp_err_to_name(ret));
    }
    
    ret = nvs_set_blob(nvs_handle, "orp_learning_gain", &g_config.orp_learning_gain, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save ORP learning gain: %s", esp_err_to_name(ret));
    }
    
    ret = nvs_set_blob(nvs_handle, "base_acid_rate", &g_config.base_acid_rate, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save base acid rate: %s", esp_err_to_name(ret));
    }
    
    // Save last pH correction percentage
    ret = nvs_set_blob(nvs_handle, "last_ph_correction", &g_state.last_ph_correction_perc, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save last pH correction: %s", esp_err_to_name(ret));
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Runtime state saved successfully");
    }
    
    return ret;
}

esp_err_t system_config_load_runtime_state(void)
{
    ESP_LOGI(TAG, "Loading runtime state from NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle for runtime state: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size;
    
    // Load acid remaining level
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "acid_remaining", &g_state.acid_remaining_ml, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored acid remaining: %.1fml", g_state.acid_remaining_ml);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load acid remaining: %s", esp_err_to_name(ret));
    }
    
    // Load chlorinator runtime hours
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "chlorinator_runtime", &g_state.chlorinator_runtime_hours, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored chlorinator runtime: %.1fh", g_state.chlorinator_runtime_hours);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load chlorinator runtime: %s", esp_err_to_name(ret));
    }
    
    // Load learning gains
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "ph_learning_gain", &g_config.ph_correction_learning_gain, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored pH learning gain: %.1f%%", g_config.ph_correction_learning_gain);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load pH learning gain: %s", esp_err_to_name(ret));
    }
    
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "orp_learning_gain", &g_config.orp_learning_gain, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored ORP learning gain: %.1f%%", g_config.orp_learning_gain);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ORP learning gain: %s", esp_err_to_name(ret));
    }
    
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "base_acid_rate", &g_config.base_acid_rate, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored base acid rate: %.1fml/h", g_config.base_acid_rate);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load base acid rate: %s", esp_err_to_name(ret));
    }
    
    // Load last pH correction percentage
    required_size = sizeof(float);
    ret = nvs_get_blob(nvs_handle, "last_ph_correction", &g_state.last_ph_correction_perc, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored last pH correction: %.1f%%", g_state.last_ph_correction_perc);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load last pH correction: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Runtime state loading completed");
    return ESP_OK;
}

esp_err_t system_config_save_learning_history(void)
{
    ESP_LOGI(TAG, "Saving learning history to NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for learning history: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save pH correction history
    ret = nvs_set_blob(nvs_handle, "ph_history", &g_state.ph_history, sizeof(g_state.ph_history));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save pH history: %s", esp_err_to_name(ret));
    }
    
    ret = nvs_set_blob(nvs_handle, "ph_history_index", &g_state.ph_history_index, sizeof(int));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save pH history index: %s", esp_err_to_name(ret));
    }
    
    // Save ORP adjustment history
    ret = nvs_set_blob(nvs_handle, "orp_history", &g_state.orp_history, sizeof(g_state.orp_history));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save ORP history: %s", esp_err_to_name(ret));
    }
    
    ret = nvs_set_blob(nvs_handle, "orp_history_index", &g_state.orp_history_index, sizeof(int));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save ORP history index: %s", esp_err_to_name(ret));
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Learning history saved successfully");
    }
    
    return ret;
}

esp_err_t system_config_load_learning_history(void)
{
    ESP_LOGI(TAG, "Loading learning history from NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle for learning history: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size;
    
    // Load pH correction history
    required_size = sizeof(g_state.ph_history);
    ret = nvs_get_blob(nvs_handle, "ph_history", &g_state.ph_history, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored pH correction history");
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load pH history: %s", esp_err_to_name(ret));
    }
    
    required_size = sizeof(int);
    ret = nvs_get_blob(nvs_handle, "ph_history_index", &g_state.ph_history_index, &required_size);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load pH history index: %s", esp_err_to_name(ret));
    }
    
    // Load ORP adjustment history
    required_size = sizeof(g_state.orp_history);
    ret = nvs_get_blob(nvs_handle, "orp_history", &g_state.orp_history, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored ORP adjustment history");
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ORP history: %s", esp_err_to_name(ret));
    }
    
    required_size = sizeof(int);
    ret = nvs_get_blob(nvs_handle, "orp_history_index", &g_state.orp_history_index, &required_size);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ORP history index: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Learning history loading completed");
    return ESP_OK;
}

void system_config_print_current(void)
{
    ESP_LOGI(TAG, "Current Configuration:");
    ESP_LOGI(TAG, "  Pool Volume: %.0fL", g_config.pool_volume_liters);
    ESP_LOGI(TAG, "  HCl Concentration: %.1f%%", g_config.hcl_concentration_percent);
    ESP_LOGI(TAG, "  pH Target: %.2f", g_config.ph_target);
    ESP_LOGI(TAG, "  ORP Target: %.0fmV", g_config.base_orp_target);
    ESP_LOGI(TAG, "  Chlorinator Duty: %.1f%%", g_config.chlorinator_duty_cycle);
    ESP_LOGI(TAG, "  Pump Thresholds: %.1f/%.1f/%.1f/%.1fA", 
             g_config.pump_thresholds.stopped_max,
             g_config.pump_thresholds.low_speed_min,
             g_config.pump_thresholds.medium_speed_min,
             g_config.pump_thresholds.high_speed_min);
    ESP_LOGI(TAG, "Runtime State:");
    ESP_LOGI(TAG, "  Acid Remaining: %.1fml", g_state.acid_remaining_ml);
    ESP_LOGI(TAG, "  Chlorinator Runtime: %.1fh", g_state.chlorinator_runtime_hours);
    ESP_LOGI(TAG, "  pH Learning Gain: %.1f%%", g_config.ph_correction_learning_gain);
    ESP_LOGI(TAG, "  ORP Learning Gain: %.1f%%", g_config.orp_learning_gain);
    ESP_LOGI(TAG, "  Base Acid Rate: %.1fml/h", g_config.base_acid_rate);
}
