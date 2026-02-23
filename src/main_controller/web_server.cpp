#include "pool_controller.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "web_assets.h"
#include "esp_random.h"

// Define MIN macro if not available
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// --- Authentication ---
static char api_token[API_TOKEN_LENGTH + 1] = {0};
static uint64_t token_last_access = 0;

// Generate a random hex token
static void generate_random_token(char *buf, size_t len)
{
    const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        buf[i] = hex[esp_random() % 16];
    }
    buf[len] = '\0';
}

bool web_server_verify_auth(httpd_req_t *req)
{
    if (api_token[0] == '\0') {
        return false;
    }

    char auth_header[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) != ESP_OK) {
        return false;
    }

    // Expect "Bearer <token>"
    if (strncmp(auth_header, "Bearer ", 7) != 0) {
        return false;
    }

    const char *token = auth_header + 7;
    if (strcmp(token, api_token) != 0) {
        return false;
    }

    // Check session timeout
    uint64_t now = get_timestamp_ms();
    if (token_last_access > 0 && (now - token_last_access) > (uint64_t)API_SESSION_TIMEOUT_S * 1000) {
        api_token[0] = '\0'; // Expire the token
        return false;
    }

    token_last_access = now;
    return true;
}

// Login endpoint - accepts password, returns token
static esp_err_t api_login_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }

    char content[128];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *pw_item = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(pw_item)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing password");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Compare against stored WiFi password as the system password
    // (In production, use a separate admin password stored in NVS)
    if (strcmp(pw_item->valuestring, g_config.wifi_password) != 0) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid password");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Generate new session token
    generate_random_token(api_token, API_TOKEN_LENGTH);
    token_last_access = get_timestamp_ms();

    httpd_resp_set_type(req, "application/json");
    char response[128];
    snprintf(response, sizeof(response), "{\"token\":\"%s\"}", api_token);
    httpd_resp_sendstr(req, response);

    cJSON_Delete(json);
    ESP_LOGI(TAG, "New API session created");
    return ESP_OK;
}

// Helper: set CORS header using request origin (not wildcard)
static void set_cors_header(httpd_req_t *req)
{
    char origin[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK) {
        // Only allow same-network origins (http:// on local IPs)
        if (strncmp(origin, "http://192.168.", 15) == 0 ||
            strncmp(origin, "http://10.", 10) == 0 ||
            strncmp(origin, "http://172.", 11) == 0 ||
            strncmp(origin, "http://localhost", 16) == 0) {
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", origin);
        }
    }
    // If no valid origin, no CORS header is set (browser blocks cross-origin)
}

// Helper: send auth error
static esp_err_t send_auth_required(httpd_req_t *req)
{
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
    return ESP_FAIL;
}

// Serve main HTML page
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    
    return ESP_OK;
}

// Serve CSS file
static esp_err_t style_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400"); // Cache for 1 day
    
    httpd_resp_send(req, (const char *)style_css_start, style_css_size);
    
    return ESP_OK;
}

// Serve JavaScript file
static esp_err_t app_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400"); // Cache for 1 day
    
    httpd_resp_send(req, (const char *)app_js_start, app_js_size);
    
    return ESP_OK;
}

// Serve PWA manifest
static esp_err_t manifest_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    
    httpd_resp_send(req, (const char *)manifest_json_start, manifest_json_size);
    
    return ESP_OK;
}

// API: Get system status (read-only, no auth required)
static esp_err_t api_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *sensors = cJSON_CreateObject();
    cJSON *equipment = cJSON_CreateObject();
    cJSON *chemical = cJSON_CreateObject();
    cJSON *learning = cJSON_CreateObject();
    cJSON *config = cJSON_CreateObject();
    cJSON *alarms = cJSON_CreateObject();
    
    // Sensor data
    cJSON_AddNumberToObject(sensors, "temperature", g_state.temperature);
    cJSON_AddNumberToObject(sensors, "ph", g_state.ph);
    cJSON_AddNumberToObject(sensors, "orp", g_state.orp);
    cJSON_AddBoolToObject(sensors, "healthy", g_state.sensors_healthy);
    
    // Equipment status
    cJSON_AddNumberToObject(equipment, "pump_status", g_state.pump_status);
    cJSON_AddNumberToObject(equipment, "pump_current", g_state.pump_current);
    cJSON_AddBoolToObject(equipment, "pump_healthy", g_state.pump_healthy);
    cJSON_AddBoolToObject(equipment, "pump_relay_on", g_state.pump_relay_on);
    cJSON_AddBoolToObject(equipment, "chlorinator_relay_on", g_state.chlorinator_relay_on);
    cJSON_AddNumberToObject(equipment, "chlorinator_current", g_state.chlorinator_current);
    cJSON_AddNumberToObject(equipment, "pump_power_watts", g_state.pump_power_watts);
    cJSON_AddNumberToObject(equipment, "pump_voltage", g_state.pump_voltage);
    cJSON_AddNumberToObject(equipment, "chlorinator_power_watts", g_state.chlorinator_power_watts);
    cJSON_AddNumberToObject(equipment, "chlorinator_voltage", g_state.chlorinator_voltage);
    cJSON_AddBoolToObject(equipment, "shelly_reachable", g_state.shelly_reachable);
    cJSON_AddNumberToObject(equipment, "shelly_temperature", g_state.shelly_temperature);
    cJSON_AddBoolToObject(equipment, "light_relay_on", g_state.light_relay_on);
    cJSON_AddNumberToObject(equipment, "current_light_mode", g_state.current_light_mode);
    
    // Chemical system
    cJSON_AddNumberToObject(chemical, "acid_remaining_ml", g_state.acid_remaining_ml);
    cJSON_AddNumberToObject(chemical, "chlorinator_runtime_hours", g_state.chlorinator_runtime_hours);
    cJSON_AddNumberToObject(chemical, "daily_consumption", get_daily_acid_consumption());
    cJSON_AddNumberToObject(chemical, "days_remaining", get_acid_days_remaining());
    
    // Learning system
    cJSON_AddNumberToObject(learning, "ph_learning_gain", g_config.ph_correction_learning_gain);
    cJSON_AddNumberToObject(learning, "orp_learning_gain", g_config.orp_learning_gain);
    cJSON_AddNumberToObject(learning, "base_acid_rate", g_config.base_acid_rate);
    cJSON_AddNumberToObject(learning, "ph_effectiveness", get_learning_effectiveness_ph());
    cJSON_AddNumberToObject(learning, "orp_effectiveness", get_learning_effectiveness_orp());
    
    // Configuration
    cJSON_AddNumberToObject(config, "ph_target", g_config.ph_target);
    cJSON_AddNumberToObject(config, "orp_target", calculate_optimal_orp_target());
    cJSON_AddNumberToObject(config, "chlorinator_duty_cycle", g_config.chlorinator_duty_cycle);
    cJSON_AddNumberToObject(config, "pool_volume_liters", g_config.pool_volume_liters);
    cJSON_AddNumberToObject(config, "hcl_concentration_percent", g_config.hcl_concentration_percent);
    
    // Alarms
    cJSON_AddBoolToObject(alarms, "pump_alarm", g_state.pump_alarm_active);
    cJSON_AddBoolToObject(alarms, "chlorinator_alarm", g_state.chlorinator_alarm_active);
    cJSON_AddBoolToObject(alarms, "acid_low_alarm", g_state.acid_low_alarm_active);
    cJSON_AddBoolToObject(alarms, "shelly_alarm", g_state.shelly_alarm_active);
    
    // Network status
    cJSON *network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "status", wifi_manager_get_status_string());
    cJSON_AddBoolToObject(network, "connected", wifi_manager_is_connected());
    if (wifi_manager_is_connected()) {
        char ip_str[16];
        wifi_manager_get_ip_string(ip_str, sizeof(ip_str));
        cJSON_AddStringToObject(network, "ip_address", ip_str);
        cJSON_AddNumberToObject(network, "signal_strength", wifi_manager_get_rssi());
        cJSON_AddStringToObject(network, "signal_description", wifi_manager_get_rssi_description(wifi_manager_get_rssi()));
    }
    
    // Add sections to main object
    cJSON_AddItemToObject(json, "sensors", sensors);
    cJSON_AddItemToObject(json, "equipment", equipment);
    cJSON_AddItemToObject(json, "chemical", chemical);
    cJSON_AddItemToObject(json, "learning", learning);
    cJSON_AddItemToObject(json, "config", config);
    cJSON_AddItemToObject(json, "alarms", alarms);
    cJSON_AddItemToObject(json, "network", network);
    cJSON_AddNumberToObject(json, "timestamp", get_timestamp_ms());
    
    char *json_string = cJSON_Print(json);
    httpd_resp_sendstr(req, json_string);
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

// API: Control lighting
static esp_err_t api_lighting_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    if (cJSON_IsNumber(mode_item)) {
        int mode = mode_item->valueint;
        if (mode >= 0 && mode < LIGHT_MODE_COUNT) {
            set_light_mode((light_mode_t)mode);

            httpd_resp_set_type(req, "application/json");
            set_cors_header(req);
            httpd_resp_sendstr(req, "{\"status\":\"success\"}");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid light mode (0-12)");
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mode parameter");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

// API: Manual acid dose
static esp_err_t api_dose_acid_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *volume_item = cJSON_GetObjectItem(json, "volume");
    if (cJSON_IsNumber(volume_item)) {
        float volume = (float)volume_item->valuedouble;

        // Input validation
        if (volume <= 0.0f || volume > 500.0f) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Volume must be 0.1-500ml");
            cJSON_Delete(json);
            return ESP_FAIL;
        }

        // Check daily limit
        if (g_state.daily_acid_dosed_ml + volume > MAX_DAILY_ACID_ML) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Would exceed daily acid limit");
            cJSON_Delete(json);
            return ESP_FAIL;
        }

        manual_acid_dose(volume);
        g_state.daily_acid_dosed_ml += volume;

        httpd_resp_set_type(req, "application/json");
        set_cors_header(req);
        httpd_resp_sendstr(req, "{\"status\":\"success\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing volume parameter");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

// API: Update configuration
static esp_err_t api_config_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    char content[500];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Update pH settings with validation
    cJSON *ph_target = cJSON_GetObjectItem(json, "ph_target");
    if (cJSON_IsNumber(ph_target)) {
        float val = (float)ph_target->valuedouble;
        if (val >= 6.8f && val <= 8.0f) {
            system_config_update_ph_settings(val,
                                            g_config.ph_correction_base_amount,
                                            g_config.ph_correction_learning_gain);
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "pH target must be 6.8-8.0");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
    }

    // Update chlorinator duty cycle with validation
    cJSON *duty_cycle = cJSON_GetObjectItem(json, "chlorinator_duty_cycle");
    if (cJSON_IsNumber(duty_cycle)) {
        float val = (float)duty_cycle->valuedouble;
        if (val >= 0.0f && val <= 100.0f) {
            system_config_update_chlorinator_settings(val, g_config.duty_cycle_period_ms);
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Duty cycle must be 0-100%");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
    }

    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_sendstr(req, "{\"status\":\"success\"}");

    cJSON_Delete(json);
    return ESP_OK;
}

// Simple polling endpoint for real-time data (read-only, no auth required)
static esp_err_t api_realtime_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    // Create JSON with current system status
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "temperature", g_state.temperature);
    cJSON_AddNumberToObject(json, "ph", g_state.ph);
    cJSON_AddNumberToObject(json, "orp", g_state.orp);
    cJSON_AddNumberToObject(json, "pump_current", g_state.pump_current);
    cJSON_AddNumberToObject(json, "chlorinator_current", g_state.chlorinator_current);
    cJSON_AddNumberToObject(json, "pump_power_watts", g_state.pump_power_watts);
    cJSON_AddNumberToObject(json, "chlorinator_power_watts", g_state.chlorinator_power_watts);
    cJSON_AddBoolToObject(json, "shelly_reachable", g_state.shelly_reachable);
    cJSON_AddBoolToObject(json, "pump_healthy", g_state.pump_healthy);
    cJSON_AddBoolToObject(json, "light_relay_on", g_state.light_relay_on);
    cJSON_AddNumberToObject(json, "current_light_mode", g_state.current_light_mode);
    cJSON_AddNumberToObject(json, "acid_remaining_ml", g_state.acid_remaining_ml);
    cJSON_AddNumberToObject(json, "timestamp", get_timestamp_ms());
    
    char *json_string = cJSON_Print(json);
    httpd_resp_sendstr(req, json_string);
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

// API: Emergency stop (auth required for security)
static esp_err_t api_emergency_stop_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    // Use centralized emergency stop
    emergency_stop_activate();

    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Emergency stop activated\"}");

    ESP_LOGW(TAG, "Emergency stop activated via web interface");
    return ESP_OK;
}

// API: Reset settings to defaults
static esp_err_t api_reset_settings_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    system_config_reset_to_defaults();
    system_config_save();

    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Settings reset to defaults\"}");

    ESP_LOGI(TAG, "Settings reset to defaults via web interface");
    return ESP_OK;
}

// API: Sensor calibration
static esp_err_t api_sensor_calibration_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);
    {
        char content[200];
        size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
        
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
            return ESP_FAIL;
        }
        content[ret] = '\0';
        
        cJSON *json = cJSON_Parse(content);
        if (json == NULL) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON *action_item = cJSON_GetObjectItem(json, "action");
        if (!cJSON_IsString(action_item)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action parameter");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        
        const char *action = action_item->valuestring;
        esp_err_t result = ESP_FAIL;
        char response_msg[200] = "";
        
        if (strcmp(action, "calibrate_ph") == 0) {
            cJSON *value_item = cJSON_GetObjectItem(json, "buffer_value");
            if (cJSON_IsNumber(value_item)) {
                float buffer_value = (float)value_item->valuedouble;
                result = atlas_calibrate_ph(buffer_value);
                snprintf(response_msg, sizeof(response_msg), 
                        "pH calibration started with buffer value %.2f", buffer_value);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing buffer_value parameter");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
        } else if (strcmp(action, "calibrate_orp") == 0) {
            cJSON *value_item = cJSON_GetObjectItem(json, "standard_mv");
            if (cJSON_IsNumber(value_item)) {
                float standard_mv = (float)value_item->valuedouble;
                result = atlas_calibrate_orp(standard_mv);
                snprintf(response_msg, sizeof(response_msg), 
                        "ORP calibration started with standard value %.0fmV", standard_mv);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing standard_mv parameter");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
        } else if (strcmp(action, "clear_ph_calibration") == 0) {
            result = atlas_clear_ph_calibration();
            snprintf(response_msg, sizeof(response_msg), "pH calibration cleared");
        } else if (strcmp(action, "clear_orp_calibration") == 0) {
            result = atlas_clear_orp_calibration();
            snprintf(response_msg, sizeof(response_msg), "ORP calibration cleared");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        
        // Send response
        httpd_resp_set_type(req, "application/json");
        set_cors_header(req);

        cJSON *response_json = cJSON_CreateObject();
        cJSON_AddStringToObject(response_json, "status", (result == ESP_OK) ? "success" : "error");
        cJSON_AddStringToObject(response_json, "message", response_msg);

        char *response_string = cJSON_Print(response_json);
        httpd_resp_sendstr(req, response_string);

        free(response_string);
        cJSON_Delete(response_json);
        cJSON_Delete(json);
    }

    return ESP_OK;
}

// API: Refill acid bottle
static esp_err_t api_refill_acid_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);
    {
        char content[100];
        size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
        
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
            return ESP_FAIL;
        }
        content[ret] = '\0';
        
        cJSON *json = cJSON_Parse(content);
        if (json == NULL) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON *volume_item = cJSON_GetObjectItem(json, "volume");
        if (cJSON_IsNumber(volume_item)) {
            float volume = (float)volume_item->valuedouble;
            if (volume > 0 && volume <= 10000) {
                refill_acid_bottle(volume);
                
                httpd_resp_set_type(req, "application/json");
                set_cors_header(req);

                char response[100];
                snprintf(response, sizeof(response),
                        "{\"status\":\"success\",\"message\":\"Acid bottle refilled to %.0fml\"}", volume);
                httpd_resp_sendstr(req, response);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid volume (must be 1-10000ml)");
            }
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing volume parameter");
        }

        cJSON_Delete(json);
    }

    return ESP_OK;
}

// API: Pump calibration
static esp_err_t api_pump_calibration_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);
    {
        char content[200];
        size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
        
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
            return ESP_FAIL;
        }
        content[ret] = '\0';
        
        cJSON *json = cJSON_Parse(content);
        if (json == NULL) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON *action_item = cJSON_GetObjectItem(json, "action");
        if (!cJSON_IsString(action_item)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action parameter");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        
        const char *action = action_item->valuestring;
        esp_err_t result = ESP_FAIL;
        char response_msg[200] = "";
        
        if (strcmp(action, "start") == 0) {
            cJSON *volume_item = cJSON_GetObjectItem(json, "target_volume");
            if (cJSON_IsNumber(volume_item)) {
                float volume = (float)volume_item->valuedouble;
                result = atlas_calibrate_pump_volume(volume);
                snprintf(response_msg, sizeof(response_msg), 
                        "Pump calibration started with %.2fml target. Measure actual volume dispensed.", volume);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing target_volume parameter");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
        } else if (strcmp(action, "complete") == 0) {
            cJSON *actual_item = cJSON_GetObjectItem(json, "actual_volume");
            if (cJSON_IsNumber(actual_item)) {
                float actual = (float)actual_item->valuedouble;
                result = atlas_set_pump_calibration(actual);
                snprintf(response_msg, sizeof(response_msg), 
                        "Pump calibration completed with %.2fml actual volume.", actual);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing actual_volume parameter");
                cJSON_Delete(json);
                return ESP_FAIL;
            }
        } else if (strcmp(action, "clear") == 0) {
            result = atlas_clear_pump_calibration();
            snprintf(response_msg, sizeof(response_msg), "Pump calibration cleared - factory defaults restored.");
        } else if (strcmp(action, "status") == 0) {
            char cal_status[64];
            result = atlas_get_pump_calibration_status(cal_status, sizeof(cal_status));
            if (result == ESP_OK) {
                snprintf(response_msg, sizeof(response_msg), "Calibration status: %s", cal_status);
            }
        } else if (strcmp(action, "total_volume") == 0) {
            float total_volume;
            result = atlas_get_pump_total_volume(&total_volume);
            if (result == ESP_OK) {
                snprintf(response_msg, sizeof(response_msg), "Total volume dispensed: %.2fml", total_volume);
            }
        } else if (strcmp(action, "clear_total") == 0) {
            result = atlas_clear_pump_total_volume();
            snprintf(response_msg, sizeof(response_msg), "Total volume counter cleared.");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        
        // Send response
        httpd_resp_set_type(req, "application/json");
        set_cors_header(req);

        cJSON *response_json = cJSON_CreateObject();
        cJSON_AddStringToObject(response_json, "status", (result == ESP_OK) ? "success" : "error");
        cJSON_AddStringToObject(response_json, "message", response_msg);

        char *response_string = cJSON_Print(response_json);
        httpd_resp_sendstr(req, response_string);

        free(response_string);
        cJSON_Delete(response_json);
        cJSON_Delete(json);
    }

    return ESP_OK;
}

// API: Configure Shelly 2PM Pro settings
static esp_err_t api_shelly_config_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    char content[256];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ip_item = cJSON_GetObjectItem(json, "ip");
    cJSON *user_item = cJSON_GetObjectItem(json, "user");
    cJSON *pass_item = cJSON_GetObjectItem(json, "password");

    const char *ip = cJSON_IsString(ip_item) ? ip_item->valuestring : g_config.shelly_ip;
    const char *user = cJSON_IsString(user_item) ? user_item->valuestring : g_config.shelly_user;
    const char *pass = cJSON_IsString(pass_item) ? pass_item->valuestring : g_config.shelly_password;

    if (strlen(ip) < 7 || strlen(ip) > 15) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid IP address");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    system_config_update_shelly_settings(ip, user, pass);

    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Shelly settings updated. Restart required.\"}");

    cJSON_Delete(json);
    ESP_LOGI(TAG, "Shelly settings updated via web interface: IP=%s", ip);
    return ESP_OK;
}

// Send real-time data (placeholder - WebSocket not supported in this ESP-IDF version)
void web_server_send_realtime_data(void)
{
    // Real-time data is now available via /api/realtime endpoint
    // Web clients can poll this endpoint for updates
}

// API: Reset emergency stop (auth required)
static esp_err_t api_emergency_reset_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    if (!web_server_verify_auth(req)) return send_auth_required(req);

    emergency_stop_reset();

    httpd_resp_set_type(req, "application/json");
    set_cors_header(req);
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"message\":\"Emergency stop reset\"}");

    ESP_LOGW(TAG, "Emergency stop reset via web interface");
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Initializing Web Server...");
    // Auth token starts empty and all protected endpoints require login first.
    api_token[0] = '\0';
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    ESP_LOGI(TAG, "Starting Web Server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.stack_size = 8192;
    config.max_uri_handlers = 17;

    if (httpd_start(&server, &config) == ESP_OK) {
        
        // Static file handlers
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t style_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &style_uri);
        
        httpd_uri_t app_js_uri = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = app_js_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &app_js_uri);
        
        httpd_uri_t manifest_uri = {
            .uri = "/manifest.json",
            .method = HTTP_GET,
            .handler = manifest_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &manifest_uri);
        
        // API handlers
        httpd_uri_t api_status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = api_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_status_uri);
        
        httpd_uri_t api_lighting_uri = {
            .uri = "/api/lighting",
            .method = HTTP_POST,
            .handler = api_lighting_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_lighting_uri);
        
        httpd_uri_t api_dose_uri = {
            .uri = "/api/dose_acid",
            .method = HTTP_POST,
            .handler = api_dose_acid_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_dose_uri);
        
        httpd_uri_t api_config_uri = {
            .uri = "/api/config",
            .method = HTTP_POST,
            .handler = api_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_config_uri);
        
        httpd_uri_t api_pump_cal_uri = {
            .uri = "/api/pump_calibration",
            .method = HTTP_POST,
            .handler = api_pump_calibration_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_pump_cal_uri);
        
        httpd_uri_t api_emergency_stop_uri = {
            .uri = "/api/emergency_stop",
            .method = HTTP_POST,
            .handler = api_emergency_stop_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_emergency_stop_uri);
        
        httpd_uri_t api_reset_settings_uri = {
            .uri = "/api/reset_settings",
            .method = HTTP_POST,
            .handler = api_reset_settings_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_reset_settings_uri);
        
        httpd_uri_t api_sensor_cal_uri = {
            .uri = "/api/sensor_calibration",
            .method = HTTP_POST,
            .handler = api_sensor_calibration_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_sensor_cal_uri);
        
        httpd_uri_t api_refill_acid_uri = {
            .uri = "/api/refill_acid",
            .method = HTTP_POST,
            .handler = api_refill_acid_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_refill_acid_uri);
        
        // Real-time data endpoint (replaces WebSocket)
        httpd_uri_t api_realtime_uri = {
            .uri = "/api/realtime",
            .method = HTTP_GET,
            .handler = api_realtime_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_realtime_uri);

        // Login endpoint
        httpd_uri_t api_login_uri = {
            .uri = "/api/login",
            .method = HTTP_POST,
            .handler = api_login_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_login_uri);

        // Emergency reset endpoint
        httpd_uri_t api_emergency_reset_uri = {
            .uri = "/api/emergency_reset",
            .method = HTTP_POST,
            .handler = api_emergency_reset_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_emergency_reset_uri);

        // Shelly config endpoint
        httpd_uri_t api_shelly_config_uri = {
            .uri = "/api/shelly_config",
            .method = HTTP_POST,
            .handler = api_shelly_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_shelly_config_uri);

        ESP_LOGI(TAG, "Web Server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start Web Server");
    return ESP_FAIL;
}
