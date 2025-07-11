#include "pool_controller.h"

static const char *TAG = "CURRENT_SENSORS";

// ACS712-5A specifications
#define ACS712_SENSITIVITY_MV_PER_A 185.0f  // 185 mV/A for ACS712-5A
#define ACS712_ZERO_CURRENT_VOLTAGE 2.5f    // 2.5V at 0A
#define ADC_VREF 3.3f                        // ESP32 ADC reference voltage
#define ADC_RESOLUTION 4095.0f               // 12-bit ADC resolution

esp_err_t current_sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing Current Sensors...");
    
    // Configure ADC1
    esp_err_t ret = adc1_config_width(ADC_WIDTH_BIT_12);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC width: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure pump current sensor channel
    ret = adc1_config_channel_atten(ACS712_PUMP_PIN, ADC_ATTEN_DB_11);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure pump current ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure chlorinator current sensor channel
    ret = adc1_config_channel_atten(ACS712_CHLORINATOR_PIN, ADC_ATTEN_DB_11);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure chlorinator current ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Current Sensors Initialized");
    return ESP_OK;
}

float read_pump_current(void)
{
    // Read multiple samples for better accuracy
    uint32_t adc_sum = 0;
    const int num_samples = 100;
    
    for (int i = 0; i < num_samples; i++) {
        adc_sum += adc1_get_raw(ACS712_PUMP_PIN);
        vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between samples
    }
    
    // Calculate average ADC reading
    float adc_average = (float)adc_sum / num_samples;
    
    // Convert ADC reading to voltage
    float voltage = (adc_average / ADC_RESOLUTION) * ADC_VREF;
    
    // Convert voltage to current using ACS712 specifications
    float current = (voltage - ACS712_ZERO_CURRENT_VOLTAGE) / (ACS712_SENSITIVITY_MV_PER_A / 1000.0f);
    
    // Take absolute value (AC current measurement)
    current = fabsf(current);
    
    // Apply simple low-pass filter to smooth readings
    static float filtered_current = 0.0f;
    const float filter_alpha = 0.1f; // Adjust for more/less filtering
    filtered_current = (filter_alpha * current) + ((1.0f - filter_alpha) * filtered_current);
    
    return filtered_current;
}

float read_chlorinator_current(void)
{
    // Read multiple samples for better accuracy
    uint32_t adc_sum = 0;
    const int num_samples = 100;
    
    for (int i = 0; i < num_samples; i++) {
        adc_sum += adc1_get_raw(ACS712_CHLORINATOR_PIN);
        vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between samples
    }
    
    // Calculate average ADC reading
    float adc_average = (float)adc_sum / num_samples;
    
    // Convert ADC reading to voltage
    float voltage = (adc_average / ADC_RESOLUTION) * ADC_VREF;
    
    // Convert voltage to current using ACS712 specifications
    float current = (voltage - ACS712_ZERO_CURRENT_VOLTAGE) / (ACS712_SENSITIVITY_MV_PER_A / 1000.0f);
    
    // Take absolute value (AC current measurement)
    current = fabsf(current);
    
    // Apply simple low-pass filter to smooth readings
    static float filtered_current = 0.0f;
    const float filter_alpha = 0.1f; // Adjust for more/less filtering
    filtered_current = (filter_alpha * current) + ((1.0f - filter_alpha) * filtered_current);
    
    return filtered_current;
}

float read_rms_current(adc1_channel_t channel, int samples, int sample_period_us)
{
    // Read RMS current for more accurate AC measurement
    float sum_squares = 0.0f;
    
    for (int i = 0; i < samples; i++) {
        // Read ADC value
        int adc_reading = adc1_get_raw(channel);
        
        // Convert to voltage
        float voltage = (adc_reading / ADC_RESOLUTION) * ADC_VREF;
        
        // Convert to current
        float current = (voltage - ACS712_ZERO_CURRENT_VOLTAGE) / (ACS712_SENSITIVITY_MV_PER_A / 1000.0f);
        
        // Square the current value
        sum_squares += current * current;
        
        // Wait for next sample
        esp_rom_delay_us(sample_period_us);
    }
    
    // Calculate RMS
    float rms_current = sqrtf(sum_squares / samples);
    
    return rms_current;
}

void calibrate_current_sensor_zero(adc1_channel_t channel)
{
    ESP_LOGI(TAG, "Calibrating current sensor zero point...");
    
    // Read multiple samples to get average zero point
    uint32_t adc_sum = 0;
    const int num_samples = 1000;
    
    for (int i = 0; i < num_samples; i++) {
        adc_sum += adc1_get_raw(channel);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    float adc_average = (float)adc_sum / num_samples;
    float zero_voltage = (adc_average / ADC_RESOLUTION) * ADC_VREF;
    
    ESP_LOGI(TAG, "Current sensor zero point: %.3fV (expected: %.3fV)", 
             zero_voltage, ACS712_ZERO_CURRENT_VOLTAGE);
    
    // TODO: Store calibration offset in NVS if needed
}

esp_err_t test_current_sensors(void)
{
    ESP_LOGI(TAG, "Testing current sensors...");
    
    // Test pump current sensor
    float pump_current = read_pump_current();
    ESP_LOGI(TAG, "Pump current: %.3fA", pump_current);
    
    // Test chlorinator current sensor
    float chlorinator_current = read_chlorinator_current();
    ESP_LOGI(TAG, "Chlorinator current: %.3fA", chlorinator_current);
    
    // Check if readings are reasonable
    if (pump_current < 0.0f || pump_current > 20.0f) {
        ESP_LOGW(TAG, "Pump current reading out of range: %.3fA", pump_current);
    }
    
    if (chlorinator_current < 0.0f || chlorinator_current > 2.0f) {
        ESP_LOGW(TAG, "Chlorinator current reading out of range: %.3fA", chlorinator_current);
    }
    
    return ESP_OK;
}
