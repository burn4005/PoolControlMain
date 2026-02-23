#include "hmi_main.h"
#include "cJSON.h"

static const char *TAG = "COMMUNICATION";

// Forward declarations
static void process_received_data(const char* json_string);
static void parse_system_data(cJSON *json);
static void parse_command_acknowledgment(cJSON *json);
static void parse_alarm_data(cJSON *json);

// UART buffer
static char uart_buffer[UART_BUF_SIZE];
static int uart_buffer_pos = 0;

esp_err_t communication_init(void)
{
    ESP_LOGI(TAG, "Initializing UART communication...");
    
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Install UART driver
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "UART communication initialized");
    return ESP_OK;
}

void communication_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Communication task started");
    
    while (1) {
        // Read data from UART
        int len = uart_read_bytes(UART_PORT_NUM, (uint8_t*)uart_buffer + uart_buffer_pos, 
                                  UART_BUF_SIZE - uart_buffer_pos - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            uart_buffer_pos += len;
            uart_buffer[uart_buffer_pos] = '\0';
            
            // Look for complete JSON messages (terminated by newline)
            char *line_start = uart_buffer;
            char *line_end;
            
            while ((line_end = strchr(line_start, '\n')) != NULL) {
                *line_end = '\0';
                
                // Process the complete line
                if (strlen(line_start) > 0) {
                    process_received_data(line_start);
                }
                
                line_start = line_end + 1;
            }
            
            // Move remaining data to beginning of buffer
            if (line_start > uart_buffer) {
                int remaining = uart_buffer_pos - (line_start - uart_buffer);
                if (remaining > 0) {
                    memmove(uart_buffer, line_start, remaining);
                }
                uart_buffer_pos = remaining;
            }
            
            // Reset buffer if it's getting full
            if (uart_buffer_pos >= UART_BUF_SIZE - 100) {
                ESP_LOGW(TAG, "UART buffer overflow, resetting");
                uart_buffer_pos = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void process_received_data(const char* json_string)
{
    ESP_LOGD(TAG, "Received: %s", json_string);
    
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse JSON: %s", json_string);
        return;
    }
    
    // Check if this is system data update
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (type != NULL && cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "system_data") == 0) {
            parse_system_data(json);
        } else if (strcmp(type->valuestring, "command_ack") == 0) {
            parse_command_acknowledgment(json);
        } else if (strcmp(type->valuestring, "alarm") == 0) {
            parse_alarm_data(json);
        }
    }
    
    cJSON_Delete(json);
}

static void parse_system_data(cJSON *json)
{
    // Parse sensor readings
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data == NULL) return;
    
    cJSON *item;
    
    // Sensor readings
    if ((item = cJSON_GetObjectItem(data, "temperature")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.temperature = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "ph")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.ph = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "orp")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.orp = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "pump_current")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.pump_current = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "chlorinator_current")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.chlorinator_current = (float)item->valuedouble;
    }
    
    // Equipment status
    if ((item = cJSON_GetObjectItem(data, "pump_relay_on")) != NULL && cJSON_IsBool(item)) {
        g_system_data.pump_relay_on = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "chlorinator_relay_on")) != NULL && cJSON_IsBool(item)) {
        g_system_data.chlorinator_relay_on = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "light_relay_on")) != NULL && cJSON_IsBool(item)) {
        g_system_data.light_relay_on = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "pump_status")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.pump_status = item->valueint;
    }
    if ((item = cJSON_GetObjectItem(data, "pump_healthy")) != NULL && cJSON_IsBool(item)) {
        g_system_data.pump_healthy = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "current_light_mode")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.current_light_mode = item->valueint;
    }
    
    // Chemical system
    if ((item = cJSON_GetObjectItem(data, "acid_remaining_ml")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.acid_remaining_ml = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "chlorinator_runtime_hours")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.chlorinator_runtime_hours = (float)item->valuedouble;
    }
    
    // Learning system
    if ((item = cJSON_GetObjectItem(data, "ph_correction_learning_gain")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.ph_correction_learning_gain = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "orp_learning_gain")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.orp_learning_gain = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "base_acid_rate")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.base_acid_rate = (float)item->valuedouble;
    }
    
    // Configuration
    if ((item = cJSON_GetObjectItem(data, "ph_target")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.ph_target = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "orp_target")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.orp_target = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "chlorinator_duty_cycle")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.chlorinator_duty_cycle = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "pool_volume_liters")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.pool_volume_liters = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(data, "hcl_concentration_percent")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.hcl_concentration_percent = (float)item->valuedouble;
    }
    
    // Alarms
    if ((item = cJSON_GetObjectItem(data, "pump_alarm_active")) != NULL && cJSON_IsBool(item)) {
        g_system_data.pump_alarm_active = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "chlorinator_alarm_active")) != NULL && cJSON_IsBool(item)) {
        g_system_data.chlorinator_alarm_active = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "acid_low_alarm_active")) != NULL && cJSON_IsBool(item)) {
        g_system_data.acid_low_alarm_active = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(data, "sensors_healthy")) != NULL && cJSON_IsBool(item)) {
        g_system_data.sensors_healthy = cJSON_IsTrue(item);
    }
    
    // Timestamp
    if ((item = cJSON_GetObjectItem(data, "timestamp")) != NULL && cJSON_IsNumber(item)) {
        g_system_data.timestamp = (uint64_t)item->valuedouble;
    }
    
    // Mark data as updated
    g_data_updated = true;
    
    ESP_LOGD(TAG, "System data updated");
}

static void parse_command_acknowledgment(cJSON *json)
{
    cJSON *command = cJSON_GetObjectItem(json, "command");
    cJSON *status = cJSON_GetObjectItem(json, "status");
    
    if (command != NULL && status != NULL && cJSON_IsString(command) && cJSON_IsString(status)) {
        ESP_LOGI(TAG, "Command '%s' %s", command->valuestring, status->valuestring);
    }
}

static void parse_alarm_data(cJSON *json)
{
    cJSON *alarm_id = cJSON_GetObjectItem(json, "alarm_id");
    cJSON *message = cJSON_GetObjectItem(json, "message");
    cJSON *active = cJSON_GetObjectItem(json, "active");
    
    if (alarm_id != NULL && message != NULL && cJSON_IsString(alarm_id) && cJSON_IsString(message)) {
        bool is_active = (active != NULL && cJSON_IsTrue(active));
        ESP_LOGW(TAG, "Alarm [%s]: %s (%s)", alarm_id->valuestring, message->valuestring, 
                 is_active ? "ACTIVE" : "CLEARED");
    }
}

esp_err_t send_command(const char* command, const char* parameter, float value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *cmd = cJSON_CreateString(command);
    cJSON *param = cJSON_CreateString(parameter);
    cJSON *val = cJSON_CreateNumber(value);
    
    cJSON_AddItemToObject(json, "command", cmd);
    cJSON_AddItemToObject(json, "parameter", param);
    cJSON_AddItemToObject(json, "value", val);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    // Add newline for message termination
    char message[512];
    snprintf(message, sizeof(message), "%s\n", json_string);
    
    int len = uart_write_bytes(UART_PORT_NUM, message, strlen(message));
    
    free(json_string);
    cJSON_Delete(json);
    
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send command");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent command: %s", command);
    return ESP_OK;
}

esp_err_t send_command_bool(const char* command, const char* parameter, bool value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *cmd = cJSON_CreateString(command);
    cJSON *param = cJSON_CreateString(parameter);
    cJSON *val = cJSON_CreateBool(value);
    
    cJSON_AddItemToObject(json, "command", cmd);
    cJSON_AddItemToObject(json, "parameter", param);
    cJSON_AddItemToObject(json, "value", val);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    char message[512];
    snprintf(message, sizeof(message), "%s\n", json_string);
    
    int len = uart_write_bytes(UART_PORT_NUM, message, strlen(message));
    
    free(json_string);
    cJSON_Delete(json);
    
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send command");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent command: %s", command);
    return ESP_OK;
}

esp_err_t send_command_string(const char* command, const char* parameter, const char* string_value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *cmd = cJSON_CreateString(command);
    cJSON *param = cJSON_CreateString(parameter);
    cJSON *val = cJSON_CreateString(string_value);
    
    cJSON_AddItemToObject(json, "command", cmd);
    cJSON_AddItemToObject(json, "parameter", param);
    cJSON_AddItemToObject(json, "value", val);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }
    
    char message[512];
    snprintf(message, sizeof(message), "%s\n", json_string);
    
    int len = uart_write_bytes(UART_PORT_NUM, message, strlen(message));
    
    free(json_string);
    cJSON_Delete(json);
    
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send command");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent command: %s = %s", command, string_value);
    return ESP_OK;
}
