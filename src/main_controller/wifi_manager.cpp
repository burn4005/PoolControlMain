#include "pool_controller.h"

static const char *TAG = "WIFI_MANAGER";

// WiFi Configuration - Hard coded credentials
#define WIFI_SSID "YourPoolWiFi"
#define WIFI_PASSWORD "YourPoolPassword"
#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool wifi_connected = false;
static bool wifi_initialized = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi station started, attempting connection...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGW(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAXIMUM_RETRY);
        }
        wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi station
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    
    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    // Configure WiFi
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished. Connecting to SSID: %s", WIFI_SSID);
    
    wifi_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_manager_connect(void)
{
    if (!wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Attempting to connect to WiFi...");
    
    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi event");
        return ESP_ERR_INVALID_STATE;
    }
}

bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}

const char* wifi_manager_get_status_string(void)
{
    return wifi_connected ? "online" : "offline";
}

esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t* ip_info)
{
    if (!wifi_connected || !ip_info) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_netif_get_ip_info(netif, ip_info);
}

void wifi_manager_get_ip_string(char* ip_str, size_t max_len)
{
    if (!ip_str || max_len < 16) {
        return;
    }
    
    if (!wifi_connected) {
        strncpy(ip_str, "Not connected", max_len - 1);
        ip_str[max_len - 1] = '\0';
        return;
    }
    
    esp_netif_ip_info_t ip_info;
    if (wifi_manager_get_ip_info(&ip_info) == ESP_OK) {
        snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        strncpy(ip_str, "IP unavailable", max_len - 1);
        ip_str[max_len - 1] = '\0';
    }
}

int wifi_manager_get_rssi(void)
{
    if (!wifi_connected) {
        return -100; // Very weak signal indicator when not connected
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        return ap_info.rssi;
    }
    
    return -100;
}

const char* wifi_manager_get_rssi_description(int rssi)
{
    if (rssi >= -30) return "Excellent";
    if (rssi >= -50) return "Very Good";
    if (rssi >= -60) return "Good";
    if (rssi >= -70) return "Fair";
    if (rssi >= -80) return "Weak";
    return "Very Weak";
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi...");
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        wifi_connected = false;
    }
    return ret;
}

esp_err_t wifi_manager_reconnect(void)
{
    if (!wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Reconnecting to WiFi...");
    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    return esp_wifi_connect();
}

void wifi_manager_status_report(void)
{
    ESP_LOGI(TAG, "=== WiFi Status Report ===");
    ESP_LOGI(TAG, "Initialized: %s", wifi_initialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Connected: %s", wifi_connected ? "Yes" : "No");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Status: %s", wifi_manager_get_status_string());
    
    if (wifi_connected) {
        char ip_str[16];
        wifi_manager_get_ip_string(ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "IP Address: %s", ip_str);
        
        int rssi = wifi_manager_get_rssi();
        ESP_LOGI(TAG, "Signal Strength: %d dBm (%s)", rssi, wifi_manager_get_rssi_description(rssi));
    }
    ESP_LOGI(TAG, "========================");
}

void wifi_manager_maintenance_task(void)
{
    static uint32_t last_status_report = 0;
    static uint32_t last_connection_check = 0;
    uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    
    // Check connection status every 30 seconds
    if ((current_time - last_connection_check) >= 30) {
        if (wifi_initialized && !wifi_connected) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnection...");
            wifi_manager_reconnect();
        }
        last_connection_check = current_time;
    }
    
    // Status report every hour
    if ((current_time - last_status_report) >= 3600) {
        wifi_manager_status_report();
        last_status_report = current_time;
    }
}

// Network status for HMI and web interface
typedef struct {
    bool connected;
    char status[16];
    char ip_address[16];
    int signal_strength;
    char signal_description[16];
} network_status_t;

network_status_t wifi_manager_get_network_status(void)
{
    network_status_t status;
    
    status.connected = wifi_connected;
    strncpy(status.status, wifi_manager_get_status_string(), sizeof(status.status) - 1);
    status.status[sizeof(status.status) - 1] = '\0';
    
    wifi_manager_get_ip_string(status.ip_address, sizeof(status.ip_address));
    
    status.signal_strength = wifi_manager_get_rssi();
    strncpy(status.signal_description, wifi_manager_get_rssi_description(status.signal_strength), sizeof(status.signal_description) - 1);
    status.signal_description[sizeof(status.signal_description) - 1] = '\0';
    
    return status;
}
