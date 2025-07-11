#include "pool_controller.h"

static const char *TAG = "NTP_SYNC";

// NTP configuration
static bool ntp_initialized = false;
static bool time_synchronized = false;

esp_err_t ntp_sync_init(void)
{
    ESP_LOGI(TAG, "Initializing NTP time synchronization...");
    
    if (ntp_initialized) {
        ESP_LOGW(TAG, "NTP already initialized");
        return ESP_OK;
    }
    
    // Set timezone
    char timezone_str[32];
    snprintf(timezone_str, sizeof(timezone_str), "UTC%+d", g_config.timezone_offset_hours);
    setenv("TZ", timezone_str, 1);
    tzset();
    
    ESP_LOGI(TAG, "Timezone set to: %s", timezone_str);
    
    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, g_config.ntp_server_primary);
    esp_sntp_setservername(1, g_config.ntp_server_backup);
    
    ESP_LOGI(TAG, "NTP servers configured: %s, %s", g_config.ntp_server_primary, g_config.ntp_server_backup);
    
    // Set sync mode and callback
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
        time_synchronized = true;
        ESP_LOGI(TAG, "Time synchronized via NTP");
        
        // Log current time
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);
    });
    
    // Start SNTP
    esp_sntp_init();
    
    ntp_initialized = true;
    ESP_LOGI(TAG, "NTP synchronization initialized successfully");
    
    return ESP_OK;
}

void ntp_sync_time(void)
{
    if (!ntp_initialized) {
        ESP_LOGW(TAG, "NTP not initialized, cannot sync time");
        return;
    }
    
    ESP_LOGI(TAG, "Requesting NTP time synchronization...");
    
    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year < (2024 - 1900)) {
        ESP_LOGW(TAG, "Time not synchronized yet, will retry later");
        time_synchronized = false;
    } else {
        time_synchronized = true;
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Time synchronized: %s", strftime_buf);
    }
}

uint64_t get_timestamp_ms(void)
{
    if (!time_synchronized) {
        // Return milliseconds since boot if time not synchronized
        return esp_timer_get_time() / 1000;
    }
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

bool is_time_synchronized(void)
{
    return time_synchronized && (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED);
}

esp_err_t set_system_time(time_t timestamp)
{
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    
    esp_err_t ret = settimeofday(&tv, NULL);
    if (ret == ESP_OK) {
        time_synchronized = true;
        ESP_LOGI(TAG, "System time set manually");
        
        // Log the set time
        struct tm timeinfo;
        localtime_r(&timestamp, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Time set to: %s", strftime_buf);
    } else {
        ESP_LOGE(TAG, "Failed to set system time: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

void get_current_time_string(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 20) {
        return;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synchronized && timeinfo.tm_year >= (2024 - 1900)) {
        strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "Time not synced");
    }
}

void get_current_date_string(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 11) {
        return;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synchronized && timeinfo.tm_year >= (2024 - 1900)) {
        strftime(buffer, buffer_size, "%Y-%m-%d", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "Unknown");
    }
}

int get_current_hour(void)
{
    if (!time_synchronized) {
        return 12; // Default to noon if time not synchronized
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    return timeinfo.tm_hour;
}

int get_current_day_of_week(void)
{
    if (!time_synchronized) {
        return 0; // Default to Sunday if time not synchronized
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    return timeinfo.tm_wday; // 0 = Sunday, 1 = Monday, etc.
}

bool is_daytime(void)
{
    int hour = get_current_hour();
    return (hour >= 6 && hour < 18); // 6 AM to 6 PM considered daytime
}

bool is_nighttime(void)
{
    int hour = get_current_hour();
    return (hour >= 22 || hour < 6); // 10 PM to 6 AM considered nighttime
}

uint32_t get_seconds_since_midnight(void)
{
    if (!time_synchronized) {
        return 0;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    return timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
}

esp_err_t update_ntp_servers(const char* primary_server, const char* backup_server)
{
    if (!primary_server || !backup_server) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update configuration
    strncpy(g_config.ntp_server_primary, primary_server, sizeof(g_config.ntp_server_primary) - 1);
    strncpy(g_config.ntp_server_backup, backup_server, sizeof(g_config.ntp_server_backup) - 1);
    g_config.ntp_server_primary[sizeof(g_config.ntp_server_primary) - 1] = '\0';
    g_config.ntp_server_backup[sizeof(g_config.ntp_server_backup) - 1] = '\0';
    
    // Restart SNTP with new servers
    if (ntp_initialized) {
        esp_sntp_stop();
        esp_sntp_setservername(0, g_config.ntp_server_primary);
        esp_sntp_setservername(1, g_config.ntp_server_backup);
        esp_sntp_init();
        
        ESP_LOGI(TAG, "NTP servers updated: %s, %s", primary_server, backup_server);
    }
    
    return ESP_OK;
}

esp_err_t update_timezone(int offset_hours)
{
    if (offset_hours < -12 || offset_hours > 14) {
        ESP_LOGE(TAG, "Invalid timezone offset: %d", offset_hours);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_config.timezone_offset_hours = offset_hours;
    
    // Update timezone
    char timezone_str[32];
    snprintf(timezone_str, sizeof(timezone_str), "UTC%+d", offset_hours);
    setenv("TZ", timezone_str, 1);
    tzset();
    
    ESP_LOGI(TAG, "Timezone updated to: %s", timezone_str);
    
    return ESP_OK;
}

void ntp_status_report(void)
{
    ESP_LOGI(TAG, "=== NTP Status Report ===");
    ESP_LOGI(TAG, "Initialized: %s", ntp_initialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Time synchronized: %s", time_synchronized ? "Yes" : "No");
    ESP_LOGI(TAG, "SNTP sync status: %d", sntp_get_sync_status());
    ESP_LOGI(TAG, "Primary NTP server: %s", g_config.ntp_server_primary);
    ESP_LOGI(TAG, "Backup NTP server: %s", g_config.ntp_server_backup);
    ESP_LOGI(TAG, "Timezone offset: UTC%+d", g_config.timezone_offset_hours);
    
    char time_str[32];
    get_current_time_string(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "Current time: %s", time_str);
    
    ESP_LOGI(TAG, "Current hour: %d", get_current_hour());
    ESP_LOGI(TAG, "Day of week: %d", get_current_day_of_week());
    ESP_LOGI(TAG, "Is daytime: %s", is_daytime() ? "Yes" : "No");
    ESP_LOGI(TAG, "Seconds since midnight: %lu", get_seconds_since_midnight());
    ESP_LOGI(TAG, "========================");
}

void ntp_maintenance_task(void)
{
    static uint32_t last_sync_attempt = 0;
    static uint32_t last_status_report = 0;
    uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    
    // Attempt sync every hour if not synchronized
    if (!time_synchronized && (current_time - last_sync_attempt) >= 3600) {
        ESP_LOGI(TAG, "Attempting periodic NTP sync...");
        ntp_sync_time();
        last_sync_attempt = current_time;
    }
    
    // Status report every 24 hours
    if ((current_time - last_status_report) >= 86400) {
        ntp_status_report();
        last_status_report = current_time;
    }
}
