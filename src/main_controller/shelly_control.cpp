#include "pool_controller.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "SHELLY_CONTROL";

// Shelly communication settings
#define SHELLY_HTTP_TIMEOUT_MS   5000
#define SHELLY_POLL_INTERVAL_MS  2000
#define SHELLY_CMD_WAIT_MS       200
#define SHELLY_CMD_QUEUE_DEPTH   8
#define SHELLY_TASK_STACK_SIZE   6144
#define SHELLY_TASK_PRIORITY     4
#define SHELLY_FAILURE_THRESHOLD 5
#define SHELLY_RESPONSE_BUF_SIZE 512
#define SHELLY_URL_BUF_SIZE      128

// Command types for the queue
typedef enum {
    SHELLY_CMD_SET_SWITCH,
} shelly_cmd_type_t;

// Command structure pushed to queue
typedef struct {
    shelly_cmd_type_t type;
    uint8_t channel;
    bool on;
} shelly_command_t;

// Overall Shelly state (shared between shelly_task and main loop)
typedef struct {
    shelly_channel_status_t channels[2];
    bool reachable;
    uint32_t consecutive_failures;
    uint64_t last_successful_poll_ms;
} shelly_state_t;

static QueueHandle_t shelly_cmd_queue = NULL;
static SemaphoreHandle_t shelly_state_mutex = NULL;
static shelly_state_t shelly_state = {};
static TaskHandle_t shelly_task_handle = NULL;

// HTTP response handling
typedef struct {
    char *buffer;
    int buffer_len;
    int data_len;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_t *resp = (http_response_t *)evt->user_data;
    if (!resp) return ESP_OK;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (resp->data_len + evt->data_len < resp->buffer_len) {
                memcpy(resp->buffer + resp->data_len, evt->data, evt->data_len);
                resp->data_len += evt->data_len;
                resp->buffer[resp->data_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t shelly_http_get(const char *rpc_path, char *response_buf, size_t buf_size)
{
    char url[SHELLY_URL_BUF_SIZE];
    snprintf(url, sizeof(url), "http://%s/rpc/%s", g_config.shelly_ip, rpc_path);

    http_response_t resp = {};
    resp.buffer = response_buf;
    resp.buffer_len = (int)buf_size;
    resp.data_len = 0;
    response_buf[0] = '\0';

    esp_http_client_config_t config = {};
    memset(&config, 0, sizeof(config));
    config.url = url;
    config.timeout_ms = SHELLY_HTTP_TIMEOUT_MS;
    config.event_handler = http_event_handler;
    config.user_data = &resp;

    // Configure digest auth if credentials are set
    if (g_config.shelly_user[0] != '\0') {
        config.username = g_config.shelly_user;
        config.password = g_config.shelly_password;
        config.auth_type = HTTP_AUTH_TYPE_DIGEST;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        return err;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "Shelly HTTP %d for %s", status_code, rpc_path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t shelly_set_switch(uint8_t channel, bool on)
{
    char path[64];
    snprintf(path, sizeof(path), "Switch.Set?id=%d&on=%s", channel, on ? "true" : "false");

    char response[128];
    esp_err_t ret = shelly_http_get(path, response, sizeof(response));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Shelly switch %d set to %s", channel, on ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "Failed to set Shelly switch %d: %s", channel, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t shelly_get_channel_status(uint8_t channel, shelly_channel_status_t *status)
{
    char path[64];
    snprintf(path, sizeof(path), "Switch.GetStatus?id=%d", channel);

    char response[SHELLY_RESPONSE_BUF_SIZE];
    esp_err_t ret = shelly_http_get(path, response, sizeof(response));
    if (ret != ESP_OK) return ret;

    cJSON *json = cJSON_Parse(response);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse Shelly response for channel %d", channel);
        return ESP_FAIL;
    }

    cJSON *output_item = cJSON_GetObjectItem(json, "output");
    cJSON *apower_item = cJSON_GetObjectItem(json, "apower");
    cJSON *current_item = cJSON_GetObjectItem(json, "current");
    cJSON *voltage_item = cJSON_GetObjectItem(json, "voltage");
    cJSON *temp_obj = cJSON_GetObjectItem(json, "temperature");

    if (cJSON_IsBool(output_item)) status->output = cJSON_IsTrue(output_item);
    if (cJSON_IsNumber(apower_item)) status->apower = (float)apower_item->valuedouble;
    if (cJSON_IsNumber(current_item)) status->current = (float)current_item->valuedouble;
    if (cJSON_IsNumber(voltage_item)) status->voltage = (float)voltage_item->valuedouble;

    if (temp_obj) {
        cJSON *tc = cJSON_GetObjectItem(temp_obj, "tC");
        if (cJSON_IsNumber(tc)) status->temperature = (float)tc->valuedouble;
    }

    status->valid = true;
    status->last_update_ms = get_timestamp_ms();

    cJSON_Delete(json);
    return ESP_OK;
}

static void shelly_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Shelly communication task started");

    TickType_t last_poll_time = xTaskGetTickCount();
    const TickType_t poll_interval = pdMS_TO_TICKS(SHELLY_POLL_INTERVAL_MS);

    while (1) {
        shelly_command_t cmd;

        // Check for commands with a short timeout to allow regular polling
        if (xQueueReceive(shelly_cmd_queue, &cmd, pdMS_TO_TICKS(SHELLY_CMD_WAIT_MS)) == pdTRUE) {
            if (cmd.type == SHELLY_CMD_SET_SWITCH) {
                esp_err_t ret = shelly_set_switch(cmd.channel, cmd.on);
                if (ret == ESP_OK) {
                    // Brief delay then poll status to confirm
                    vTaskDelay(pdMS_TO_TICKS(500));
                    shelly_channel_status_t ch_status = {};
                    if (shelly_get_channel_status(cmd.channel, &ch_status) == ESP_OK) {
                        xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
                        shelly_state.channels[cmd.channel] = ch_status;
                        shelly_state.reachable = true;
                        shelly_state.consecutive_failures = 0;
                        shelly_state.last_successful_poll_ms = get_timestamp_ms();
                        xSemaphoreGive(shelly_state_mutex);
                    }
                } else {
                    xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
                    shelly_state.consecutive_failures++;
                    if (shelly_state.consecutive_failures >= SHELLY_FAILURE_THRESHOLD) {
                        shelly_state.reachable = false;
                    }
                    xSemaphoreGive(shelly_state_mutex);
                }
            }
        }

        // Periodic polling - every 2 seconds, poll both channels
        if ((xTaskGetTickCount() - last_poll_time) >= poll_interval) {
            last_poll_time = xTaskGetTickCount();

            bool poll_ok = true;
            for (int ch = 0; ch < 2; ch++) {
                shelly_channel_status_t ch_status = {};
                if (shelly_get_channel_status(ch, &ch_status) == ESP_OK) {
                    xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
                    shelly_state.channels[ch] = ch_status;
                    xSemaphoreGive(shelly_state_mutex);
                } else {
                    poll_ok = false;
                }
            }

            xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
            if (poll_ok) {
                shelly_state.reachable = true;
                shelly_state.consecutive_failures = 0;
                shelly_state.last_successful_poll_ms = get_timestamp_ms();
            } else {
                shelly_state.consecutive_failures++;
                if (shelly_state.consecutive_failures >= SHELLY_FAILURE_THRESHOLD) {
                    shelly_state.reachable = false;
                }
            }
            xSemaphoreGive(shelly_state_mutex);
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

esp_err_t shelly_control_init(void)
{
    ESP_LOGI(TAG, "Initializing Shelly Control...");

    if (g_config.shelly_ip[0] == '\0') {
        ESP_LOGW(TAG, "Shelly IP not configured - Shelly control disabled");
        return ESP_ERR_INVALID_STATE;
    }

    shelly_state_mutex = xSemaphoreCreateMutex();
    shelly_cmd_queue = xQueueCreate(SHELLY_CMD_QUEUE_DEPTH, sizeof(shelly_command_t));

    if (!shelly_state_mutex || !shelly_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create Shelly synchronization primitives");
        return ESP_ERR_NO_MEM;
    }

    memset(&shelly_state, 0, sizeof(shelly_state_t));

    BaseType_t ret = xTaskCreate(shelly_task, "shelly_task",
                                 SHELLY_TASK_STACK_SIZE, NULL,
                                 SHELLY_TASK_PRIORITY, &shelly_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Shelly task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Shelly Control Initialized - IP: %s", g_config.shelly_ip);
    return ESP_OK;
}

esp_err_t shelly_queue_switch(uint8_t channel, bool on)
{
    if (!shelly_cmd_queue) {
        ESP_LOGE(TAG, "Shelly not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    shelly_command_t cmd = {};
    cmd.type = SHELLY_CMD_SET_SWITCH;
    cmd.channel = channel;
    cmd.on = on;

    if (xQueueSend(shelly_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue Shelly command (queue full)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void shelly_get_cached_state(shelly_channel_status_t *pump_status, shelly_channel_status_t *chlorinator_status, bool *reachable)
{
    if (!shelly_state_mutex) {
        if (pump_status) memset(pump_status, 0, sizeof(shelly_channel_status_t));
        if (chlorinator_status) memset(chlorinator_status, 0, sizeof(shelly_channel_status_t));
        if (reachable) *reachable = false;
        return;
    }

    xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
    if (pump_status) *pump_status = shelly_state.channels[SHELLY_CH_PUMP];
    if (chlorinator_status) *chlorinator_status = shelly_state.channels[SHELLY_CH_CHLORINATOR];
    if (reachable) *reachable = shelly_state.reachable;
    xSemaphoreGive(shelly_state_mutex);
}

bool shelly_is_reachable(void)
{
    if (!shelly_state_mutex) return false;

    bool reachable;
    xSemaphoreTake(shelly_state_mutex, portMAX_DELAY);
    reachable = shelly_state.reachable;
    xSemaphoreGive(shelly_state_mutex);
    return reachable;
}
