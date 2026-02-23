#include "pool_controller.h"

#include "cJSON.h"
#include "strings.h"

static const char *TAG = "HMI_COMM";

#define HMI_UART_NUM UART_NUM_2
#define HMI_UART_BAUD_RATE 115200
#define HMI_BUF_SIZE 1024
#define HMI_TX_LINE_MAX 512
#define HMI_RX_LINE_MAX 512

static char s_rx_line[HMI_RX_LINE_MAX];
static size_t s_rx_len;
static uint64_t s_last_data_send_time_ms;

static esp_err_t hmi_send_json_line(cJSON *json)
{
    if (json == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char line[HMI_TX_LINE_MAX];
    const int written = snprintf(line, sizeof(line), "%s\n", json_str);
    free(json_str);

    if (written < 0 || written >= (int)sizeof(line)) {
        ESP_LOGE(TAG, "Outgoing JSON exceeded %u bytes", (unsigned)sizeof(line));
        return ESP_ERR_INVALID_SIZE;
    }

    if (uart_write_bytes(HMI_UART_NUM, line, written) < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t hmi_send_command_ack(const char *command, bool ok, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "type", "command_ack");
    cJSON_AddStringToObject(root, "command", command != NULL ? command : "unknown");
    cJSON_AddStringToObject(root, "status", ok ? "ok" : "error");
    if (message != NULL && message[0] != '\0') {
        cJSON_AddStringToObject(root, "message", message);
    }

    esp_err_t ret = hmi_send_json_line(root);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t hmi_send_system_data(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if (root == NULL || data == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "type", "system_data");

    cJSON_AddNumberToObject(data, "temperature", g_state.temperature);
    cJSON_AddNumberToObject(data, "ph", g_state.ph);
    cJSON_AddNumberToObject(data, "orp", g_state.orp);
    cJSON_AddNumberToObject(data, "pump_current", g_state.pump_current);
    cJSON_AddNumberToObject(data, "chlorinator_current", g_state.chlorinator_current);

    cJSON_AddBoolToObject(data, "pump_relay_on", g_state.pump_relay_on);
    cJSON_AddBoolToObject(data, "chlorinator_relay_on", g_state.chlorinator_relay_on);
    cJSON_AddBoolToObject(data, "light_relay_on", g_state.light_relay_on);
    cJSON_AddBoolToObject(data, "pump_healthy", g_state.pump_healthy);
    cJSON_AddBoolToObject(data, "pump_alarm_active", g_state.pump_alarm_active);
    cJSON_AddBoolToObject(data, "chlorinator_alarm_active", g_state.chlorinator_alarm_active);
    cJSON_AddBoolToObject(data, "acid_low_alarm_active", g_state.acid_low_alarm_active);
    cJSON_AddBoolToObject(data, "sensors_healthy", g_state.sensors_healthy);

    cJSON_AddNumberToObject(data, "pump_status", (int)g_state.pump_status);
    cJSON_AddNumberToObject(data, "current_light_mode", (int)g_state.current_light_mode);

    cJSON_AddNumberToObject(data, "acid_remaining_ml", g_state.acid_remaining_ml);
    cJSON_AddNumberToObject(data, "chlorinator_runtime_hours", g_state.chlorinator_runtime_hours);
    cJSON_AddNumberToObject(data, "ph_correction_learning_gain", g_config.ph_correction_learning_gain);
    cJSON_AddNumberToObject(data, "orp_learning_gain", g_config.orp_learning_gain);
    cJSON_AddNumberToObject(data, "base_acid_rate", g_config.base_acid_rate);

    cJSON_AddNumberToObject(data, "ph_target", g_config.ph_target);
    cJSON_AddNumberToObject(data, "orp_target", calculate_optimal_orp_target());
    cJSON_AddNumberToObject(data, "chlorinator_duty_cycle", g_config.chlorinator_duty_cycle);
    cJSON_AddNumberToObject(data, "pool_volume_liters", g_config.pool_volume_liters);
    cJSON_AddNumberToObject(data, "hcl_concentration_percent", g_config.hcl_concentration_percent);

    cJSON_AddNumberToObject(data, "timestamp", (double)get_timestamp_ms());

    cJSON_AddItemToObject(root, "data", data);

    esp_err_t ret = hmi_send_json_line(root);
    cJSON_Delete(root);
    return ret;
}

static light_mode_t parse_light_mode_string(const char *mode)
{
    if (mode == NULL) {
        return LIGHT_OFF;
    }

    static const char *k_names[LIGHT_MODE_COUNT] = {
        "off", "blue", "pink", "red", "yellow", "green", "cyan", "white",
        "mode 1", "mode 2", "mode 3", "mode 4", "brightness"
    };

    for (int i = 0; i < LIGHT_MODE_COUNT; i++) {
        if (strcasecmp(mode, k_names[i]) == 0) {
            return (light_mode_t)i;
        }
    }

    return LIGHT_OFF;
}

static esp_err_t hmi_process_command_json(cJSON *json)
{
    cJSON *cmd_item = cJSON_GetObjectItem(json, "command");
    if (!cJSON_IsString(cmd_item) || cmd_item->valuestring == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *cmd = cmd_item->valuestring;
    cJSON *value_item = cJSON_GetObjectItem(json, "value");
    esp_err_t result = ESP_OK;

    if (strcmp(cmd, "set_pump_relay") == 0) {
        if (!cJSON_IsBool(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            set_pump_relay(cJSON_IsTrue(value_item));
        }
    } else if (strcmp(cmd, "set_chlorinator_relay") == 0) {
        if (!cJSON_IsBool(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            set_chlorinator_relay(cJSON_IsTrue(value_item));
        }
    } else if (strcmp(cmd, "set_light_relay") == 0) {
        if (!cJSON_IsBool(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            if (cJSON_IsTrue(value_item)) {
                const light_mode_t new_mode = (g_state.current_light_mode == LIGHT_OFF) ? LIGHT_BLUE : g_state.current_light_mode;
                set_light_mode(new_mode);
            } else {
                set_light_mode(LIGHT_OFF);
            }
        }
    } else if (strcmp(cmd, "manual_dose") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            const float volume = (float)value_item->valuedouble;
            if (volume <= 0.0f || volume > 500.0f) {
                result = ESP_ERR_INVALID_ARG;
            } else {
                manual_acid_dose(volume);
            }
        }
    } else if (strcmp(cmd, "set_light_mode") == 0) {
        if (!cJSON_IsString(value_item) || value_item->valuestring == NULL) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            set_light_mode(parse_light_mode_string(value_item->valuestring));
        }
    } else if (strcmp(cmd, "set_ph_target") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = system_config_update_ph_settings((float)value_item->valuedouble,
                                                      g_config.ph_correction_base_amount,
                                                      g_config.ph_correction_learning_gain);
        }
    } else if (strcmp(cmd, "set_orp_target") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = system_config_update_orp_settings((float)value_item->valuedouble,
                                                       g_config.orp_temp_coefficient,
                                                       g_config.orp_learning_gain,
                                                       g_config.orp_adjustment_interval_ms);
        }
    } else if (strcmp(cmd, "set_chlorinator_duty") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = system_config_update_chlorinator_settings((float)value_item->valuedouble,
                                                               g_config.duty_cycle_period_ms);
        }
    } else if (strcmp(cmd, "pump_cal_start") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = atlas_calibrate_pump_volume((float)value_item->valuedouble);
        }
    } else if (strcmp(cmd, "pump_cal_complete") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = atlas_set_pump_calibration((float)value_item->valuedouble);
        }
    } else if (strcmp(cmd, "calibrate_ph") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = atlas_calibrate_ph((float)value_item->valuedouble);
        }
    } else if (strcmp(cmd, "calibrate_orp") == 0) {
        if (!cJSON_IsNumber(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            result = atlas_calibrate_orp((float)value_item->valuedouble);
        }
    } else if (strcmp(cmd, "ack_alarm") == 0) {
        result = ESP_OK;
    } else if (strcmp(cmd, "emergency_stop") == 0) {
        if (!cJSON_IsBool(value_item) || !cJSON_IsTrue(value_item)) {
            result = ESP_ERR_INVALID_ARG;
        } else {
            emergency_stop_activate();
        }
    } else {
        result = ESP_ERR_NOT_SUPPORTED;
    }

    const bool ok = (result == ESP_OK);
    hmi_send_command_ack(cmd, ok, ok ? "" : esp_err_to_name(result));
    return result;
}

esp_err_t hmi_communication_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI communication (JSON over UART)");

    uart_config_t uart_config = {};
    uart_config.baud_rate = HMI_UART_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t ret = uart_driver_install(HMI_UART_NUM, HMI_BUF_SIZE * 2, HMI_BUF_SIZE * 2, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_param_config(HMI_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_set_pin(HMI_UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_rx_len = 0;
    s_last_data_send_time_ms = 0;
    ESP_LOGI(TAG, "HMI UART ready on TX=%d RX=%d", UART_TX_PIN, UART_RX_PIN);
    return ESP_OK;
}

void hmi_send_data(void)
{
    const uint64_t now_ms = get_timestamp_ms();
    if ((now_ms - s_last_data_send_time_ms) < 1000ULL) {
        return;
    }

    if (hmi_send_system_data() == ESP_OK) {
        s_last_data_send_time_ms = now_ms;
    }
}

void hmi_process_commands(void)
{
    uint8_t rx[128];
    int len = 0;

    do {
        len = uart_read_bytes(HMI_UART_NUM, rx, sizeof(rx), 0);
        for (int i = 0; i < len; i++) {
            const char ch = (char)rx[i];
            if (ch == '\r') {
                continue;
            }

            if (ch == '\n') {
                if (s_rx_len > 0) {
                    s_rx_line[s_rx_len] = '\0';
                    cJSON *json = cJSON_Parse(s_rx_line);
                    if (json != NULL) {
                        hmi_process_command_json(json);
                        cJSON_Delete(json);
                    } else {
                        ESP_LOGW(TAG, "Invalid HMI JSON command: %s", s_rx_line);
                    }
                    s_rx_len = 0;
                }
                continue;
            }

            if (s_rx_len + 1 < sizeof(s_rx_line)) {
                s_rx_line[s_rx_len++] = ch;
            } else {
                ESP_LOGW(TAG, "Dropping oversized HMI line");
                s_rx_len = 0;
            }
        }
    } while (len > 0);
}
