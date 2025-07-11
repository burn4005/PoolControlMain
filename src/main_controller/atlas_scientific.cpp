#include "pool_controller.h"

static const char *TAG = "ATLAS_SCIENTIFIC";

// I2C configuration
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000

// Atlas Scientific command delays
#define ATLAS_RESPONSE_DELAY_MS 1000
#define ATLAS_CALIBRATION_DELAY_MS 1300
#define ATLAS_READING_DELAY_MS 900

// Command strings
#define ATLAS_CMD_READ "R"
#define ATLAS_CMD_INFO "I"
#define ATLAS_CMD_STATUS "STATUS"
#define ATLAS_CMD_FACTORY_RESET "FACTORY"
#define ATLAS_CMD_SLEEP "SLEEP"
#define ATLAS_CMD_FIND "FIND"

// pH/ORP specific commands
#define ATLAS_CMD_PH_CAL_MID "Cal,mid,7.00"
#define ATLAS_CMD_PH_CAL_LOW "Cal,low,4.00"
#define ATLAS_CMD_PH_CAL_HIGH "Cal,high,10.00"
#define ATLAS_CMD_PH_CAL_CLEAR "Cal,clear"
#define ATLAS_CMD_ORP_CAL "Cal,%d"
#define ATLAS_CMD_ORP_CAL_CLEAR "Cal,clear"

// Temperature compensation
#define ATLAS_CMD_TEMP_COMP "T,%.2f"

// Pump commands
#define ATLAS_CMD_PUMP_DOSE "D,%.2f"
#define ATLAS_CMD_PUMP_STOP "X"
#define ATLAS_CMD_PUMP_PAUSE "P"
#define ATLAS_CMD_PUMP_STATUS "D,?"

// Response codes
#define ATLAS_RESPONSE_SUCCESS 1
#define ATLAS_RESPONSE_SYNTAX_ERROR 2
#define ATLAS_RESPONSE_NOT_READY 254
#define ATLAS_RESPONSE_NO_DATA 255

esp_err_t atlas_scientific_init(void)
{
    ESP_LOGI(TAG, "Initializing Atlas Scientific I2C interface...");
    
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_PIN;
    conf.scl_io_num = I2C_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C parameters: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Test communication with each device
    ESP_LOGI(TAG, "Testing communication with Atlas Scientific devices...");
    
    // Test pH/ORP sensor
    char response[32];
    if (atlas_send_command(EZO_PH_ORP_ADDR, ATLAS_CMD_INFO, response, sizeof(response)) == ESP_OK) {
        ESP_LOGI(TAG, "pH/ORP sensor found: %s", response);
    } else {
        ESP_LOGW(TAG, "pH/ORP sensor not responding");
    }
    
    // Test temperature sensor
    if (atlas_send_command(EZO_RTD_ADDR, ATLAS_CMD_INFO, response, sizeof(response)) == ESP_OK) {
        ESP_LOGI(TAG, "Temperature sensor found: %s", response);
    } else {
        ESP_LOGW(TAG, "Temperature sensor not responding");
    }
    
    // Test dosing pump
    if (atlas_send_command(EZO_PMP_ADDR, ATLAS_CMD_INFO, response, sizeof(response)) == ESP_OK) {
        ESP_LOGI(TAG, "Dosing pump found: %s", response);
    } else {
        ESP_LOGW(TAG, "Dosing pump not responding");
    }
    
    ESP_LOGI(TAG, "Atlas Scientific initialization complete");
    return ESP_OK;
}

esp_err_t atlas_send_command(uint8_t device_addr, const char* command, char* response, size_t response_size)
{
    esp_err_t ret;
    
    // Send command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t*)command, strlen(command), true);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command to device 0x%02X: %s", device_addr, esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(ATLAS_RESPONSE_DELAY_MS));
    
    // Read response
    uint8_t response_code;
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &response_code, I2C_MASTER_ACK);
    
    if (response && response_size > 1) {
        i2c_master_read(cmd, (uint8_t*)response, response_size - 1, I2C_MASTER_LAST_NACK);
    }
    
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read response from device 0x%02X: %s", device_addr, esp_err_to_name(ret));
        return ret;
    }
    
    // Check response code
    if (response_code != ATLAS_RESPONSE_SUCCESS) {
        ESP_LOGW(TAG, "Device 0x%02X returned error code: %d", device_addr, response_code);
        return ESP_FAIL;
    }
    
    // Null-terminate response if buffer provided
    if (response && response_size > 0) {
        response[response_size - 1] = '\0';
        
        // Remove any trailing whitespace
        int len = strlen(response);
        while (len > 0 && (response[len-1] == '\r' || response[len-1] == '\n' || response[len-1] == ' ')) {
            response[--len] = '\0';
        }
    }
    
    return ESP_OK;
}

float atlas_read_ph_orp_ph(void)
{
    char response[16];
    char command[32];
    
    // Set temperature compensation first
    snprintf(command, sizeof(command), ATLAS_CMD_TEMP_COMP, g_state.temperature);
    atlas_send_command(EZO_PH_ORP_ADDR, command, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(300)); // Short delay for temperature setting
    
    // Read pH value
    if (atlas_send_command(EZO_PH_ORP_ADDR, "R,pH", response, sizeof(response)) == ESP_OK) {
        float ph_value = atof(response);
        
        // Validate pH range (typical range 0-14)
        if (ph_value >= 0.0f && ph_value <= 14.0f) {
            ESP_LOGD(TAG, "pH reading: %.2f", ph_value);
            return ph_value;
        } else {
            ESP_LOGW(TAG, "Invalid pH reading: %.2f", ph_value);
        }
    } else {
        ESP_LOGW(TAG, "Failed to read pH value");
    }
    
    return 7.0f; // Return neutral pH as fallback
}

float atlas_read_ph_orp_orp(void)
{
    char response[16];
    char command[32];
    
    // Set temperature compensation first
    snprintf(command, sizeof(command), ATLAS_CMD_TEMP_COMP, g_state.temperature);
    atlas_send_command(EZO_PH_ORP_ADDR, command, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(300)); // Short delay for temperature setting
    
    // Read ORP value
    if (atlas_send_command(EZO_PH_ORP_ADDR, "R,ORP", response, sizeof(response)) == ESP_OK) {
        float orp_value = atof(response);
        
        // Validate ORP range (typical range -1000 to +1000 mV)
        if (orp_value >= -1000.0f && orp_value <= 1000.0f) {
            ESP_LOGD(TAG, "ORP reading: %.1f mV", orp_value);
            return orp_value;
        } else {
            ESP_LOGW(TAG, "Invalid ORP reading: %.1f", orp_value);
        }
    } else {
        ESP_LOGW(TAG, "Failed to read ORP value");
    }
    
    return 700.0f; // Return typical pool ORP as fallback
}

float atlas_read_temperature(void)
{
    char response[16];
    
    if (atlas_send_command(EZO_RTD_ADDR, ATLAS_CMD_READ, response, sizeof(response)) == ESP_OK) {
        float temp_value = atof(response);
        
        // Validate temperature range (typical pool range 10-40°C)
        if (temp_value >= 5.0f && temp_value <= 50.0f) {
            ESP_LOGD(TAG, "Temperature reading: %.1f°C", temp_value);
            return temp_value;
        } else {
            ESP_LOGW(TAG, "Invalid temperature reading: %.1f", temp_value);
        }
    } else {
        ESP_LOGW(TAG, "Failed to read temperature");
    }
    
    return 25.0f; // Return typical pool temperature as fallback
}

esp_err_t atlas_dose_acid(float volume_ml)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Dosing %.2fml of acid", volume_ml);
    
    // Validate volume
    if (volume_ml <= 0.0f || volume_ml > 500.0f) {
        ESP_LOGE(TAG, "Invalid dose volume: %.2fml", volume_ml);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Send dose command
    snprintf(command, sizeof(command), ATLAS_CMD_PUMP_DOSE, volume_ml);
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Acid dosing started: %s", response);
        
        // Calculate estimated dosing time (assuming 1ml/second rate)
        uint32_t dose_time_ms = (uint32_t)(volume_ml * 1000);
        
        // Log completion time
        ESP_LOGI(TAG, "Estimated dosing time: %.1f seconds", volume_ml);
        
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start acid dosing");
        return ret;
    }
}

esp_err_t atlas_stop_dosing(void)
{
    ESP_LOGI(TAG, "Stopping acid dosing");
    
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, ATLAS_CMD_PUMP_STOP, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Acid dosing stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop acid dosing");
    }
    
    return ret;
}

esp_err_t atlas_get_pump_status(char* status, size_t status_size)
{
    return atlas_send_command(EZO_PMP_ADDR, ATLAS_CMD_PUMP_STATUS, status, status_size);
}

esp_err_t atlas_calibrate_ph(float buffer_value)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Calibrating pH with buffer value: %.2f", buffer_value);
    
    // Determine calibration point based on buffer value
    if (buffer_value >= 6.5f && buffer_value <= 7.5f) {
        // Mid-point calibration (pH 7)
        strcpy(command, ATLAS_CMD_PH_CAL_MID);
    } else if (buffer_value >= 3.5f && buffer_value <= 4.5f) {
        // Low-point calibration (pH 4)
        strcpy(command, ATLAS_CMD_PH_CAL_LOW);
    } else if (buffer_value >= 9.5f && buffer_value <= 10.5f) {
        // High-point calibration (pH 10)
        strcpy(command, ATLAS_CMD_PH_CAL_HIGH);
    } else {
        ESP_LOGE(TAG, "Invalid pH buffer value: %.2f", buffer_value);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = atlas_send_command(EZO_PH_ORP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "pH calibration completed: %s", response);
        
        // Wait for calibration to complete
        vTaskDelay(pdMS_TO_TICKS(ATLAS_CALIBRATION_DELAY_MS));
    } else {
        ESP_LOGE(TAG, "pH calibration failed");
    }
    
    return ret;
}

esp_err_t atlas_calibrate_orp(float standard_mv)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Calibrating ORP with standard: %.0f mV", standard_mv);
    
    // Validate ORP standard range
    if (standard_mv < 0.0f || standard_mv > 1000.0f) {
        ESP_LOGE(TAG, "Invalid ORP standard value: %.0f mV", standard_mv);
        return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(command, sizeof(command), ATLAS_CMD_ORP_CAL, (int)standard_mv);
    esp_err_t ret = atlas_send_command(EZO_PH_ORP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ORP calibration completed: %s", response);
        
        // Wait for calibration to complete
        vTaskDelay(pdMS_TO_TICKS(ATLAS_CALIBRATION_DELAY_MS));
    } else {
        ESP_LOGE(TAG, "ORP calibration failed");
    }
    
    return ret;
}

esp_err_t atlas_clear_ph_calibration(void)
{
    ESP_LOGI(TAG, "Clearing pH calibration");
    
    esp_err_t ret = atlas_send_command(EZO_PH_ORP_ADDR, ATLAS_CMD_PH_CAL_CLEAR, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "pH calibration cleared");
    } else {
        ESP_LOGE(TAG, "Failed to clear pH calibration");
    }
    
    return ret;
}

esp_err_t atlas_clear_orp_calibration(void)
{
    ESP_LOGI(TAG, "Clearing ORP calibration");
    
    esp_err_t ret = atlas_send_command(EZO_PH_ORP_ADDR, ATLAS_CMD_ORP_CAL_CLEAR, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ORP calibration cleared");
    } else {
        ESP_LOGE(TAG, "Failed to clear ORP calibration");
    }
    
    return ret;
}

esp_err_t atlas_factory_reset(uint8_t device_addr)
{
    ESP_LOGW(TAG, "Factory resetting device 0x%02X", device_addr);
    
    esp_err_t ret = atlas_send_command(device_addr, ATLAS_CMD_FACTORY_RESET, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device 0x%02X factory reset completed", device_addr);
        
        // Wait for device to restart
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        ESP_LOGE(TAG, "Failed to factory reset device 0x%02X", device_addr);
    }
    
    return ret;
}

esp_err_t atlas_find_device(uint8_t device_addr)
{
    ESP_LOGI(TAG, "Finding device 0x%02X (LED will blink)", device_addr);
    
    esp_err_t ret = atlas_send_command(device_addr, ATLAS_CMD_FIND, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Find command sent to device 0x%02X", device_addr);
    } else {
        ESP_LOGE(TAG, "Failed to send find command to device 0x%02X", device_addr);
    }
    
    return ret;
}

esp_err_t atlas_get_device_info(uint8_t device_addr, char* info, size_t info_size)
{
    return atlas_send_command(device_addr, ATLAS_CMD_INFO, info, info_size);
}

esp_err_t atlas_sleep_device(uint8_t device_addr)
{
    ESP_LOGI(TAG, "Putting device 0x%02X to sleep", device_addr);
    
    esp_err_t ret = atlas_send_command(device_addr, ATLAS_CMD_SLEEP, NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device 0x%02X is now sleeping", device_addr);
    } else {
        ESP_LOGE(TAG, "Failed to put device 0x%02X to sleep", device_addr);
    }
    
    return ret;
}

bool atlas_is_device_ready(uint8_t device_addr)
{
    char response[16];
    
    if (atlas_send_command(device_addr, ATLAS_CMD_STATUS, response, sizeof(response)) == ESP_OK) {
        // Check if response indicates device is ready
        return (strstr(response, "P") == NULL); // P indicates powered off/sleeping
    }
    
    return false;
}

esp_err_t atlas_calibrate_pump_volume(float target_volume_ml)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Starting pump volume calibration for %.2fml", target_volume_ml);
    
    // Validate volume
    if (target_volume_ml <= 0.0f || target_volume_ml > 100.0f) {
        ESP_LOGE(TAG, "Invalid calibration volume: %.2fml (must be 0.1-100ml)", target_volume_ml);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Start calibration mode
    snprintf(command, sizeof(command), "Cal,%.2f", target_volume_ml);
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump calibration started: %s", response);
        ESP_LOGI(TAG, "Manually measure the actual dispensed volume and call atlas_set_pump_calibration()");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start pump calibration");
        return ret;
    }
}

esp_err_t atlas_set_pump_calibration(float actual_volume_ml)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Setting pump calibration with actual volume: %.2fml", actual_volume_ml);
    
    // Validate volume
    if (actual_volume_ml <= 0.0f || actual_volume_ml > 100.0f) {
        ESP_LOGE(TAG, "Invalid actual volume: %.2fml", actual_volume_ml);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set the actual volume measured
    snprintf(command, sizeof(command), "Cal,%.2f", actual_volume_ml);
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump calibration completed: %s", response);
        
        // Wait for calibration to complete
        vTaskDelay(pdMS_TO_TICKS(ATLAS_CALIBRATION_DELAY_MS));
        
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to complete pump calibration");
        return ret;
    }
}

esp_err_t atlas_clear_pump_calibration(void)
{
    ESP_LOGI(TAG, "Clearing pump calibration");
    
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, "Cal,clear", NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump calibration cleared - factory defaults restored");
    } else {
        ESP_LOGE(TAG, "Failed to clear pump calibration");
    }
    
    return ret;
}

esp_err_t atlas_get_pump_calibration_status(char* status, size_t status_size)
{
    ESP_LOGI(TAG, "Getting pump calibration status");
    
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, "Cal,?", status, status_size);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump calibration status: %s", status);
    } else {
        ESP_LOGE(TAG, "Failed to get pump calibration status");
    }
    
    return ret;
}

esp_err_t atlas_set_pump_max_volume(float max_volume_ml)
{
    char command[32];
    char response[32];
    
    ESP_LOGI(TAG, "Setting pump maximum volume to %.2fml", max_volume_ml);
    
    // Validate volume (typical range 0.1ml to 105ml for EZO-PMP)
    if (max_volume_ml < 0.1f || max_volume_ml > 105.0f) {
        ESP_LOGE(TAG, "Invalid max volume: %.2fml (must be 0.1-105ml)", max_volume_ml);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set maximum dispensing volume
    snprintf(command, sizeof(command), "M,%.2f", max_volume_ml);
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, command, response, sizeof(response));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump max volume set: %s", response);
    } else {
        ESP_LOGE(TAG, "Failed to set pump max volume");
    }
    
    return ret;
}

esp_err_t atlas_get_pump_total_volume(float* total_ml)
{
    char response[32];
    
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, "TV,?", response, sizeof(response));
    
    if (ret == ESP_OK) {
        *total_ml = atof(response);
        ESP_LOGI(TAG, "Pump total volume dispensed: %.2fml", *total_ml);
    } else {
        ESP_LOGE(TAG, "Failed to get pump total volume");
        *total_ml = 0.0f;
    }
    
    return ret;
}

esp_err_t atlas_clear_pump_total_volume(void)
{
    ESP_LOGI(TAG, "Clearing pump total volume counter");
    
    esp_err_t ret = atlas_send_command(EZO_PMP_ADDR, "TV,clear", NULL, 0);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Pump total volume counter cleared");
    } else {
        ESP_LOGE(TAG, "Failed to clear pump total volume counter");
    }
    
    return ret;
}

// Wrapper functions for compatibility
float atlas_read_ph(void)
{
    return atlas_read_ph_orp_ph();
}

float atlas_read_orp(void)
{
    return atlas_read_ph_orp_orp();
}

void atlas_maintenance_routine(void)
{
    ESP_LOGI(TAG, "Running Atlas Scientific maintenance routine...");
    
    // Check device status
    char info[64];
    
    if (atlas_get_device_info(EZO_PH_ORP_ADDR, info, sizeof(info)) == ESP_OK) {
        ESP_LOGI(TAG, "pH/ORP sensor info: %s", info);
    }
    
    if (atlas_get_device_info(EZO_RTD_ADDR, info, sizeof(info)) == ESP_OK) {
        ESP_LOGI(TAG, "Temperature sensor info: %s", info);
    }
    
    if (atlas_get_device_info(EZO_PMP_ADDR, info, sizeof(info)) == ESP_OK) {
        ESP_LOGI(TAG, "Dosing pump info: %s", info);
    }
    
    // Check pump status and calibration
    char pump_status[32];
    if (atlas_get_pump_status(pump_status, sizeof(pump_status)) == ESP_OK) {
        ESP_LOGI(TAG, "Pump status: %s", pump_status);
    }
    
    char cal_status[32];
    if (atlas_get_pump_calibration_status(cal_status, sizeof(cal_status)) == ESP_OK) {
        ESP_LOGI(TAG, "Pump calibration: %s", cal_status);
    }
    
    float total_volume;
    if (atlas_get_pump_total_volume(&total_volume) == ESP_OK) {
        ESP_LOGI(TAG, "Total volume dispensed: %.2fml", total_volume);
    }
    
    ESP_LOGI(TAG, "Atlas Scientific maintenance routine completed");
}
