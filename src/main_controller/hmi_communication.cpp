#include "pool_controller.h"
#include "esp_timer.h"

static const char *TAG = "HMI_COMM";

// UART configuration for HMI communication
#define HMI_UART_NUM UART_NUM_2
#define HMI_UART_BAUD_RATE 115200
#define HMI_UART_DATA_BITS UART_DATA_8_BITS
#define HMI_UART_PARITY UART_PARITY_DISABLE
#define HMI_UART_STOP_BITS UART_STOP_BITS_1
#define HMI_UART_FLOW_CTRL UART_HW_FLOWCTRL_DISABLE
#define HMI_UART_SOURCE_CLK UART_SCLK_DEFAULT

#define HMI_BUF_SIZE 1024
#define HMI_QUEUE_SIZE 20

// Message types
#define HMI_MSG_SENSOR_DATA 0x01
#define HMI_MSG_EQUIPMENT_STATUS 0x02
#define HMI_MSG_CHEMICAL_STATUS 0x03
#define HMI_MSG_LEARNING_STATUS 0x04
#define HMI_MSG_ALARM_STATUS 0x05
#define HMI_MSG_CONFIG_DATA 0x06
#define HMI_MSG_COMMAND 0x10
#define HMI_MSG_ACK 0x20
#define HMI_MSG_NACK 0x21

// Command types
#define HMI_CMD_SET_LIGHT_MODE 0x01
#define HMI_CMD_MANUAL_DOSE 0x02
#define HMI_CMD_EMERGENCY_STOP 0x03
#define HMI_CMD_CALIBRATE_SENSORS 0x04
#define HMI_CMD_UPDATE_CONFIG 0x05
#define HMI_CMD_REFILL_ACID 0x06
#define HMI_CMD_PUMP_CAL_START 0x07
#define HMI_CMD_PUMP_CAL_COMPLETE 0x08
#define HMI_CMD_PUMP_CAL_CLEAR 0x09
#define HMI_CMD_PUMP_CAL_STATUS 0x0A
#define HMI_CMD_CALIBRATE_PH 0x0B
#define HMI_CMD_CALIBRATE_ORP 0x0C
#define HMI_CMD_CLEAR_PH_CAL 0x0D
#define HMI_CMD_CLEAR_ORP_CAL 0x0E

// Message structure
typedef struct {
    uint8_t start_byte;     // 0xAA
    uint8_t msg_type;
    uint8_t length;
    uint8_t data[64];
    uint8_t checksum;
    uint8_t end_byte;       // 0x55
} hmi_message_t;

static QueueHandle_t hmi_uart_queue;
static uint32_t last_data_send_time = 0;
static const uint32_t HMI_DATA_SEND_INTERVAL_MS = 1000; // Send data every second

esp_err_t hmi_communication_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI communication...");
    
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = HMI_UART_BAUD_RATE,
        .data_bits = HMI_UART_DATA_BITS,
        .parity = HMI_UART_PARITY,
        .stop_bits = HMI_UART_STOP_BITS,
        .flow_ctrl = HMI_UART_FLOW_CTRL,
        .source_clk = HMI_UART_SOURCE_CLK,
    };
    
    // Install UART driver
    esp_err_t ret = uart_driver_install(HMI_UART_NUM, HMI_BUF_SIZE * 2, HMI_BUF_SIZE * 2, HMI_QUEUE_SIZE, &hmi_uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(HMI_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_set_pin(HMI_UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "HMI communication initialized successfully");
    return ESP_OK;
}

static uint8_t calculate_checksum(const uint8_t* data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

static esp_err_t hmi_send_message(uint8_t msg_type, const uint8_t* data, uint8_t data_length)
{
    if (data_length > 64) {
        ESP_LOGE(TAG, "Data length too large: %d", data_length);
        return ESP_ERR_INVALID_ARG;
    }
    
    hmi_message_t msg;
    msg.start_byte = 0xAA;
    msg.msg_type = msg_type;
    msg.length = data_length;
    
    if (data && data_length > 0) {
        memcpy(msg.data, data, data_length);
    }
    
    // Calculate checksum over message type, length, and data
    uint8_t checksum_data[66];
    checksum_data[0] = msg.msg_type;
    checksum_data[1] = msg.length;
    memcpy(&checksum_data[2], msg.data, data_length);
    msg.checksum = calculate_checksum(checksum_data, data_length + 2);
    
    msg.end_byte = 0x55;
    
    // Send message
    int bytes_written = uart_write_bytes(HMI_UART_NUM, &msg, sizeof(hmi_message_t));
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "Failed to send HMI message");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent HMI message type 0x%02X, length %d", msg_type, data_length);
    return ESP_OK;
}

static esp_err_t hmi_send_sensor_data(void)
{
    struct {
        float temperature;
        float ph;
        float orp;
        uint8_t sensors_healthy;
    } sensor_data;
    
    sensor_data.temperature = g_state.temperature;
    sensor_data.ph = g_state.ph;
    sensor_data.orp = g_state.orp;
    sensor_data.sensors_healthy = g_state.sensors_healthy ? 1 : 0;
    
    return hmi_send_message(HMI_MSG_SENSOR_DATA, (uint8_t*)&sensor_data, sizeof(sensor_data));
}

static esp_err_t hmi_send_equipment_status(void)
{
    struct {
        uint8_t pump_status;
        float pump_current;
        uint8_t pump_healthy;
        uint8_t chlorinator_relay_on;
        float chlorinator_current;
        uint8_t light_relay_on;
        uint8_t current_light_mode;
        uint8_t duty_cycle_active;
    } equipment_data;
    
    equipment_data.pump_status = (uint8_t)g_state.pump_status;
    equipment_data.pump_current = g_state.pump_current;
    equipment_data.pump_healthy = g_state.pump_healthy ? 1 : 0;
    equipment_data.chlorinator_relay_on = g_state.chlorinator_relay_on ? 1 : 0;
    equipment_data.chlorinator_current = g_state.chlorinator_current;
    equipment_data.light_relay_on = g_state.light_relay_on ? 1 : 0;
    equipment_data.current_light_mode = (uint8_t)g_state.current_light_mode;
    equipment_data.duty_cycle_active = g_state.duty_cycle_active ? 1 : 0;
    
    return hmi_send_message(HMI_MSG_EQUIPMENT_STATUS, (uint8_t*)&equipment_data, sizeof(equipment_data));
}

static esp_err_t hmi_send_chemical_status(void)
{
    struct {
        float acid_remaining_ml;
        float chlorinator_runtime_hours;
        float daily_consumption;
        uint32_t days_remaining;
    } chemical_data;
    
    chemical_data.acid_remaining_ml = g_state.acid_remaining_ml;
    chemical_data.chlorinator_runtime_hours = g_state.chlorinator_runtime_hours;
    chemical_data.daily_consumption = get_daily_acid_consumption();
    chemical_data.days_remaining = get_acid_days_remaining();
    
    return hmi_send_message(HMI_MSG_CHEMICAL_STATUS, (uint8_t*)&chemical_data, sizeof(chemical_data));
}

static esp_err_t hmi_send_learning_status(void)
{
    struct {
        float ph_learning_gain;
        float orp_learning_gain;
        float base_acid_rate;
        float ph_effectiveness;
        float orp_effectiveness;
    } learning_data;
    
    learning_data.ph_learning_gain = g_config.ph_correction_learning_gain;
    learning_data.orp_learning_gain = g_config.orp_learning_gain;
    learning_data.base_acid_rate = g_config.base_acid_rate;
    learning_data.ph_effectiveness = get_learning_effectiveness_ph();
    learning_data.orp_effectiveness = get_learning_effectiveness_orp();
    
    return hmi_send_message(HMI_MSG_LEARNING_STATUS, (uint8_t*)&learning_data, sizeof(learning_data));
}

static esp_err_t hmi_send_alarm_status(void)
{
    struct {
        uint8_t pump_alarm;
        uint8_t chlorinator_alarm;
        uint8_t acid_low_alarm;
    } alarm_data;
    
    alarm_data.pump_alarm = g_state.pump_alarm_active ? 1 : 0;
    alarm_data.chlorinator_alarm = g_state.chlorinator_alarm_active ? 1 : 0;
    alarm_data.acid_low_alarm = g_state.acid_low_alarm_active ? 1 : 0;
    
    return hmi_send_message(HMI_MSG_ALARM_STATUS, (uint8_t*)&alarm_data, sizeof(alarm_data));
}

static esp_err_t hmi_send_config_data(void)
{
    struct {
        float ph_target;
        float orp_target;
        float chlorinator_duty_cycle;
        float pool_volume_liters;
        float hcl_concentration_percent;
    } config_data;
    
    config_data.ph_target = g_config.ph_target;
    config_data.orp_target = calculate_optimal_orp_target();
    config_data.chlorinator_duty_cycle = g_config.chlorinator_duty_cycle;
    config_data.pool_volume_liters = g_config.pool_volume_liters;
    config_data.hcl_concentration_percent = g_config.hcl_concentration_percent;
    
    return hmi_send_message(HMI_MSG_CONFIG_DATA, (uint8_t*)&config_data, sizeof(config_data));
}

void hmi_send_data(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Send data at regular intervals
    if (current_time - last_data_send_time >= HMI_DATA_SEND_INTERVAL_MS) {
        hmi_send_sensor_data();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        hmi_send_equipment_status();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        hmi_send_chemical_status();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        hmi_send_learning_status();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        hmi_send_alarm_status();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        hmi_send_config_data();
        
        last_data_send_time = current_time;
    }
}

static esp_err_t hmi_process_command(const hmi_message_t* msg)
{
    if (msg->length < 1) {
        ESP_LOGW(TAG, "Command message too short");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t command = msg->data[0];
    esp_err_t result = ESP_OK;
    
    switch (command) {
        case HMI_CMD_SET_LIGHT_MODE:
            if (msg->length >= 2) {
                light_mode_t mode = (light_mode_t)msg->data[1];
                if (mode <= LIGHT_BRIGHTNESS) {
                    set_light_mode(mode);
                    ESP_LOGI(TAG, "HMI command: Set light mode to %d", mode);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_MANUAL_DOSE:
            if (msg->length >= 5) {
                float volume;
                memcpy(&volume, &msg->data[1], sizeof(float));
                if (volume > 0 && volume <= 100) {
                    manual_acid_dose(volume);
                    ESP_LOGI(TAG, "HMI command: Manual dose %.2fml", volume);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_EMERGENCY_STOP:
            // Emergency stop all equipment
            set_pump_relay(false);
            set_chlorinator_relay(false);
            atlas_stop_dosing();
            ESP_LOGW(TAG, "HMI command: Emergency stop activated");
            break;
            
        case HMI_CMD_CALIBRATE_SENSORS:
            // Trigger sensor calibration routine
            ESP_LOGI(TAG, "HMI command: Sensor calibration requested");
            // Implementation would depend on specific calibration procedure
            break;
            
        case HMI_CMD_UPDATE_CONFIG:
            if (msg->length >= 9) {
                float ph_target, duty_cycle;
                memcpy(&ph_target, &msg->data[1], sizeof(float));
                memcpy(&duty_cycle, &msg->data[5], sizeof(float));
                
                system_config_update_ph_settings(ph_target, g_config.ph_correction_base_amount, g_config.ph_correction_learning_gain);
                system_config_update_chlorinator_settings(duty_cycle, g_config.duty_cycle_period_ms);
                
                ESP_LOGI(TAG, "HMI command: Config updated - pH target: %.2f, Duty cycle: %.1f%%", ph_target, duty_cycle);
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_REFILL_ACID:
            if (msg->length >= 5) {
                float new_volume;
                memcpy(&new_volume, &msg->data[1], sizeof(float));
                if (new_volume > 0 && new_volume <= 10000) {
                    refill_acid_bottle(new_volume);
                    ESP_LOGI(TAG, "HMI command: Acid bottle refilled to %.0fml", new_volume);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_PUMP_CAL_START:
            if (msg->length >= 5) {
                float target_volume;
                memcpy(&target_volume, &msg->data[1], sizeof(float));
                if (target_volume > 0 && target_volume <= 1000) {
                    result = atlas_calibrate_pump_volume(target_volume);
                    ESP_LOGI(TAG, "HMI command: Pump calibration started with %.2fml target", target_volume);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_PUMP_CAL_COMPLETE:
            if (msg->length >= 5) {
                float actual_volume;
                memcpy(&actual_volume, &msg->data[1], sizeof(float));
                if (actual_volume > 0 && actual_volume <= 1000) {
                    result = atlas_set_pump_calibration(actual_volume);
                    ESP_LOGI(TAG, "HMI command: Pump calibration completed with %.2fml actual", actual_volume);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_PUMP_CAL_CLEAR:
            result = atlas_clear_pump_calibration();
            ESP_LOGI(TAG, "HMI command: Pump calibration cleared");
            break;
            
        case HMI_CMD_PUMP_CAL_STATUS:
            {
                char status[64];
                result = atlas_get_pump_calibration_status(status, sizeof(status));
                ESP_LOGI(TAG, "HMI command: Pump calibration status: %s", status);
            }
            break;
            
        case HMI_CMD_CALIBRATE_PH:
            if (msg->length >= 5) {
                float buffer_value;
                memcpy(&buffer_value, &msg->data[1], sizeof(float));
                if (buffer_value >= 1.0 && buffer_value <= 14.0) {
                    result = atlas_calibrate_ph(buffer_value);
                    ESP_LOGI(TAG, "HMI command: pH calibration with buffer %.2f", buffer_value);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_CALIBRATE_ORP:
            if (msg->length >= 5) {
                float standard_mv;
                memcpy(&standard_mv, &msg->data[1], sizeof(float));
                if (standard_mv >= -2000 && standard_mv <= 2000) {
                    result = atlas_calibrate_orp(standard_mv);
                    ESP_LOGI(TAG, "HMI command: ORP calibration with standard %.0fmV", standard_mv);
                } else {
                    result = ESP_ERR_INVALID_ARG;
                }
            } else {
                result = ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HMI_CMD_CLEAR_PH_CAL:
            result = atlas_clear_ph_calibration();
            ESP_LOGI(TAG, "HMI command: pH calibration cleared");
            break;
            
        case HMI_CMD_CLEAR_ORP_CAL:
            result = atlas_clear_orp_calibration();
            ESP_LOGI(TAG, "HMI command: ORP calibration cleared");
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown HMI command: 0x%02X", command);
            result = ESP_ERR_NOT_SUPPORTED;
            break;
    }
    
    // Send acknowledgment
    uint8_t ack_data = command;
    uint8_t ack_type = (result == ESP_OK) ? HMI_MSG_ACK : HMI_MSG_NACK;
    hmi_send_message(ack_type, &ack_data, 1);
    
    return result;
}

void hmi_process_commands(void)
{
    uart_event_t event;
    
    // Check for UART events
    if (xQueueReceive(hmi_uart_queue, (void*)&event, 0)) {
        if (event.type == UART_DATA) {
            uint8_t buffer[HMI_BUF_SIZE];
            int length = uart_read_bytes(HMI_UART_NUM, buffer, event.size, pdMS_TO_TICKS(100));
            
            if (length > 0) {
                // Process received data
                for (int i = 0; i < length; i++) {
                    static hmi_message_t rx_msg;
                    static int rx_state = 0;
                    static int rx_index = 0;
                    
                    uint8_t byte = buffer[i];
                    
                    switch (rx_state) {
                        case 0: // Wait for start byte
                            if (byte == 0xAA) {
                                rx_msg.start_byte = byte;
                                rx_state = 1;
                                rx_index = 0;
                            }
                            break;
                            
                        case 1: // Message type
                            rx_msg.msg_type = byte;
                            rx_state = 2;
                            break;
                            
                        case 2: // Length
                            rx_msg.length = byte;
                            if (rx_msg.length > 64) {
                                rx_state = 0; // Invalid length, reset
                            } else {
                                rx_state = 3;
                                rx_index = 0;
                            }
                            break;
                            
                        case 3: // Data
                            rx_msg.data[rx_index++] = byte;
                            if (rx_index >= rx_msg.length) {
                                rx_state = 4;
                            }
                            break;
                            
                        case 4: // Checksum
                            rx_msg.checksum = byte;
                            rx_state = 5;
                            break;
                            
                        case 5: // End byte
                            if (byte == 0x55) {
                                rx_msg.end_byte = byte;
                                
                                // Verify checksum
                                uint8_t checksum_data[66];
                                checksum_data[0] = rx_msg.msg_type;
                                checksum_data[1] = rx_msg.length;
                                memcpy(&checksum_data[2], rx_msg.data, rx_msg.length);
                                uint8_t calculated_checksum = calculate_checksum(checksum_data, rx_msg.length + 2);
                                
                                if (calculated_checksum == rx_msg.checksum) {
                                    // Valid message received
                                    if (rx_msg.msg_type == HMI_MSG_COMMAND) {
                                        hmi_process_command(&rx_msg);
                                    }
                                } else {
                                    ESP_LOGW(TAG, "HMI message checksum mismatch");
                                }
                            }
                            rx_state = 0;
                            break;
                    }
                }
            }
        }
    }
}
