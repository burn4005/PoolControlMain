#include "hmi_main.h"

#include "cJSON.h"
#include "trend_data.h"

static const char *TAG = "COMMUNICATION";

static char s_uart_buffer[UART_BUF_SIZE];
static int s_uart_buffer_pos;
static uint64_t s_last_trend_sample_ms;

static esp_err_t send_json_message(cJSON *json);

static void process_received_data(const char *json_string);
static void parse_system_data(cJSON *json);
static void parse_command_acknowledgment(cJSON *json);
static void parse_alarm_data(cJSON *json);

esp_err_t communication_init(void)
{
    uart_config_t uart_config = {};
    uart_config.baud_rate = UART_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "RS485 UART initialized on TX=%d RX=%d", UART_TX_PIN, UART_RX_PIN);
    return ESP_OK;
}

void communication_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        const int len = uart_read_bytes(UART_PORT_NUM, (uint8_t *)s_uart_buffer + s_uart_buffer_pos,
                                        UART_BUF_SIZE - s_uart_buffer_pos - 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            s_uart_buffer_pos += len;
            s_uart_buffer[s_uart_buffer_pos] = '\0';

            char *line_start = s_uart_buffer;
            char *line_end = NULL;
            while ((line_end = strchr(line_start, '\n')) != NULL) {
                *line_end = '\0';
                if (strlen(line_start) > 2) {
                    process_received_data(line_start);
                }
                line_start = line_end + 1;
            }

            if (line_start > s_uart_buffer) {
                const int remaining = s_uart_buffer_pos - (int)(line_start - s_uart_buffer);
                if (remaining > 0) {
                    memmove(s_uart_buffer, line_start, remaining);
                }
                s_uart_buffer_pos = remaining;
            }

            if (s_uart_buffer_pos > UART_BUF_SIZE - 128) {
                s_uart_buffer_pos = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void process_received_data(const char *json_string)
{
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        ESP_LOGW(TAG, "Invalid JSON: %s", json_string);
        return;
    }

    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (cJSON_IsString(type)) {
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
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data == NULL) {
        return;
    }
    float trend_ph = 0.0f;
    float trend_orp = 0.0f;
    float trend_temp = 0.0f;
    bool parsed = false;

    cJSON *item = NULL;
#define PARSE_NUM(name, field)                      \
    do {                                            \
        item = cJSON_GetObjectItem(data, name);     \
        if (cJSON_IsNumber(item)) {                 \
            g_system_data.field = (float)item->valuedouble; \
        }                                           \
    } while (0)

#define PARSE_BOOL(name, field)                     \
    do {                                            \
        item = cJSON_GetObjectItem(data, name);     \
        if (cJSON_IsBool(item)) {                   \
            g_system_data.field = cJSON_IsTrue(item); \
        }                                           \
    } while (0)

    if (xSemaphoreTake(g_system_data_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        PARSE_NUM("temperature", temperature);
        PARSE_NUM("ph", ph);
        PARSE_NUM("orp", orp);
        PARSE_NUM("pump_current", pump_current);
        PARSE_NUM("chlorinator_current", chlorinator_current);

        PARSE_BOOL("pump_relay_on", pump_relay_on);
        PARSE_BOOL("chlorinator_relay_on", chlorinator_relay_on);
        PARSE_BOOL("light_relay_on", light_relay_on);
        PARSE_BOOL("pump_healthy", pump_healthy);
        PARSE_BOOL("pump_alarm_active", pump_alarm_active);
        PARSE_BOOL("chlorinator_alarm_active", chlorinator_alarm_active);
        PARSE_BOOL("acid_low_alarm_active", acid_low_alarm_active);
        PARSE_BOOL("sensors_healthy", sensors_healthy);

        item = cJSON_GetObjectItem(data, "pump_status");
        if (cJSON_IsNumber(item)) g_system_data.pump_status = item->valueint;

        item = cJSON_GetObjectItem(data, "current_light_mode");
        if (cJSON_IsNumber(item)) g_system_data.current_light_mode = item->valueint;

        PARSE_NUM("acid_remaining_ml", acid_remaining_ml);
        PARSE_NUM("chlorinator_runtime_hours", chlorinator_runtime_hours);
        PARSE_NUM("ph_correction_learning_gain", ph_correction_learning_gain);
        PARSE_NUM("orp_learning_gain", orp_learning_gain);
        PARSE_NUM("base_acid_rate", base_acid_rate);
        PARSE_NUM("ph_target", ph_target);
        PARSE_NUM("orp_target", orp_target);
        PARSE_NUM("chlorinator_duty_cycle", chlorinator_duty_cycle);
        PARSE_NUM("pool_volume_liters", pool_volume_liters);
        PARSE_NUM("hcl_concentration_percent", hcl_concentration_percent);

        item = cJSON_GetObjectItem(data, "timestamp");
        if (cJSON_IsNumber(item)) {
            g_system_data.timestamp = (uint64_t)item->valuedouble;
        }
        trend_ph = g_system_data.ph;
        trend_orp = g_system_data.orp;
        trend_temp = g_system_data.temperature;
        g_data_updated = true;
        parsed = true;
        xSemaphoreGive(g_system_data_mutex);
    }

#undef PARSE_NUM
#undef PARSE_BOOL

    g_last_data_rx_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);

    if (parsed && (g_last_data_rx_ms - s_last_trend_sample_ms) >= 60000ULL) {
        trend_data_add_sample(trend_ph, trend_orp, trend_temp, g_last_data_rx_ms);
        s_last_trend_sample_ms = g_last_data_rx_ms;
    }
}

static void parse_command_acknowledgment(cJSON *json)
{
    cJSON *cmd = cJSON_GetObjectItem(json, "command");
    cJSON *status = cJSON_GetObjectItem(json, "status");
    if (cJSON_IsString(cmd) && cJSON_IsString(status)) {
        ESP_LOGI(TAG, "Ack %s: %s", cmd->valuestring, status->valuestring);
    }
}

static void parse_alarm_data(cJSON *json)
{
    cJSON *alarm_id = cJSON_GetObjectItem(json, "alarm_id");
    cJSON *message = cJSON_GetObjectItem(json, "message");
    if (cJSON_IsString(alarm_id) && cJSON_IsString(message)) {
        ESP_LOGW(TAG, "Alarm %s: %s", alarm_id->valuestring, message->valuestring);
    }
}

esp_err_t send_command(const char *command, const char *parameter, float value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", command);
    cJSON_AddStringToObject(json, "parameter", parameter);
    cJSON_AddNumberToObject(json, "value", value);

    esp_err_t ret = send_json_message(json);
    cJSON_Delete(json);
    return ret;
}

esp_err_t send_command_bool(const char *command, const char *parameter, bool value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", command);
    cJSON_AddStringToObject(json, "parameter", parameter);
    cJSON_AddBoolToObject(json, "value", value);

    esp_err_t ret = send_json_message(json);
    cJSON_Delete(json);
    return ret;
}

esp_err_t send_command_string(const char *command, const char *parameter, const char *string_value)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", command);
    cJSON_AddStringToObject(json, "parameter", parameter);
    cJSON_AddStringToObject(json, "value", string_value);

    esp_err_t ret = send_json_message(json);
    cJSON_Delete(json);
    return ret;
}

static esp_err_t send_json_message(cJSON *json)
{
    if (json == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *json_string = cJSON_PrintUnformatted(json);
    if (json_string == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char message[256];
    const int written = snprintf(message, sizeof(message), "%s\n", json_string);
    free(json_string);
    if (written < 0 || written >= (int)sizeof(message)) {
        ESP_LOGE(TAG, "Command JSON exceeded buffer (%d bytes)", written);
        return ESP_ERR_INVALID_SIZE;
    }

    if (uart_write_bytes(UART_PORT_NUM, message, written) < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
