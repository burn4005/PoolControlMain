#ifndef POOL_CONTROLLER_H
#define POOL_CONTROLLER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_http_client.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "time.h"

// Hardware Pin Definitions
#define LIGHT_RELAY_PIN GPIO_NUM_14
#define I2C_SDA_PIN GPIO_NUM_21
#define I2C_SCL_PIN GPIO_NUM_22
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16

// Atlas Scientific I2C Addresses
#define EZO_PH_ORP_ADDR 0x63
#define EZO_RTD_ADDR 0x66
#define EZO_PMP_ADDR 0x67

// System Configuration
#define POOL_VOLUME_LITERS 30000.0f
#define HCL_CONCENTRATION_PERCENT 32.5f
#define ACID_BOTTLE_SIZE_ML 5000.0f
#define ACID_LOW_ALERT_ML 250.0f
#define MIN_PUMP_RUNTIME_MS 120000  // 2 minutes
#define SENSOR_STABILIZATION_TIME_MS 120000  // 2 minutes

// Chemical Dosing Safety Limits
#define MIN_BASE_ACID_DOSE_ML 0.5f
#define MAX_BASE_ACID_DOSE_ML 100.0f
#define MIN_PH_CORRECTION_ML 5.0f
#define MAX_PH_CORRECTION_ML 200.0f
#define MAX_DAILY_ACID_ML 2000.0f           // Maximum acid per 24-hour period
#define ACID_ADDITION_INTERVAL_DEFAULT_MS 600000   // 10 minutes
#define PH_CORRECTION_INTERVAL_MS 1800000          // 30 minutes
#define PH_CORRECTION_PUMP_DELAY_MS 600000         // 10 min after pump start
#define PH_LEARNING_EVAL_DELAY_MS 7200000          // 2 hours
#define ORP_LEARNING_EVAL_DELAY_MS 1800000         // 30 minutes
#define BASE_ACID_LEARNING_INTERVAL_MS 86400000    // 24 hours
#define PH_ERROR_SCALING_REFERENCE 0.2f            // Scale for 0.2 pH unit error
#define PH_ERROR_SCALING_MAX 2.0f                  // Cap error scaling at 2x

// Learning System Bounds
#define PH_LEARNING_GAIN_MIN 5.0f
#define PH_LEARNING_GAIN_MAX 30.0f   // Tightened from 50%
#define ORP_LEARNING_GAIN_MIN 5.0f
#define ORP_LEARNING_GAIN_MAX 30.0f
#define LEARNING_MAX_CHANGE_PERCENT 0.20f  // Max 20% change per adjustment
#define LEARNING_EFFECTIVENESS_MIN 0.1f
#define LEARNING_EFFECTIVENESS_MAX 3.0f

// API Authentication
#define API_TOKEN_LENGTH 32
#define API_SESSION_TIMEOUT_S 3600  // 1 hour

// Pump Status Enumeration
typedef enum {
    PUMP_OFF = 0,
    PUMP_LOW_SPEED = 1,
    PUMP_MEDIUM_SPEED = 2,
    PUMP_HIGH_SPEED = 3,
    PUMP_ON_BUT_STOPPED = 4
} pump_status_t;

// Shelly 2PM Pro Channel Definitions
#define SHELLY_CH_PUMP        0
#define SHELLY_CH_CHLORINATOR 1

// Shelly Channel Status (populated by shelly_control.cpp)
typedef struct {
    bool output;
    float apower;
    float current;
    float voltage;
    float temperature;
    bool valid;
    uint64_t last_update_ms;
} shelly_channel_status_t;

// Pump Thresholds Structure (power-based via Shelly)
typedef struct {
    float stopped_max_watts;      // Maximum power for "stopped" detection
    float low_speed_min_watts;    // Minimum power for low speed
    float medium_speed_min_watts; // Minimum power for medium speed
    float high_speed_min_watts;   // Minimum power for high speed
} pump_thresholds_t;

// Light Mode Enumeration
typedef enum {
    LIGHT_OFF = 0,
    LIGHT_BLUE = 1,
    LIGHT_PINK = 2,
    LIGHT_RED = 3,
    LIGHT_YELLOW = 4,
    LIGHT_GREEN = 5,
    LIGHT_CYAN = 6,
    LIGHT_WHITE = 7,
    LIGHT_MODE1 = 8,
    LIGHT_MODE2 = 9,
    LIGHT_MODE3 = 10,
    LIGHT_MODE4 = 11,
    LIGHT_BRIGHTNESS = 12,
    LIGHT_MODE_COUNT = 13
} light_mode_t;

// Light Timing Structure
typedef struct {
    light_mode_t mode;
    const char* name;
    uint16_t off_time_ms;
    const char* description;
} light_timing_t;

// pH Correction History Structure
typedef struct {
    float ph_before;
    float ph_after;
    float correction_amount;
    float expected_change;
    float actual_change;
    float effectiveness_ratio;
    uint64_t timestamp;
} ph_correction_history_t;

// ORP Adjustment History Structure
typedef struct {
    float orp_before;
    float orp_after;
    float duty_adjustment;
    float expected_orp_change;
    float actual_orp_change;
    float effectiveness_ratio;
    uint64_t timestamp;
} orp_adjustment_history_t;

// System Configuration Structure
typedef struct {
    // Pool Configuration
    float pool_volume_liters;
    float hcl_concentration_percent;
    float acid_bottle_size_ml;
    float acid_low_alert_ml;
    
    // pH Control
    float ph_target;
    float ph_correction_base_amount;
    float ph_correction_learning_gain;
    float base_acid_rate;
    float base_acid_learning_gain;
    float learning_rate;
    
    // ORP Control
    float base_orp_target;
    float orp_temp_coefficient;
    float orp_learning_gain;
    uint32_t orp_adjustment_interval_ms;
    
    // Chlorinator Control
    float chlorinator_duty_cycle;
    uint32_t duty_cycle_period_ms;
    uint32_t acid_addition_interval_ms;
    
    // Pump Thresholds
    pump_thresholds_t pump_thresholds;
    
    // Current Monitoring
    float chlorinator_min_current;
    float chlorinator_max_current;
    
    // Time Settings
    int timezone_offset_hours;
    char ntp_server_primary[64];
    char ntp_server_backup[64];
    
    // WiFi Settings
    char wifi_ssid[32];
    char wifi_password[64];

    // Shelly 2PM Pro Settings
    char shelly_ip[16];
    char shelly_user[32];
    char shelly_password[64];
} system_config_t;

// System State Structure
typedef struct {
    // Sensor Readings
    float temperature;
    float ph;
    float orp;
    float pump_current;
    float chlorinator_current;
    float pump_power_watts;
    float pump_voltage;
    float chlorinator_power_watts;
    float chlorinator_voltage;
    float shelly_temperature;
    bool shelly_reachable;
    bool shelly_alarm_active;

    // Equipment Status
    bool pump_relay_on;
    bool chlorinator_relay_on;
    bool light_relay_on;
    pump_status_t pump_status;
    bool pump_healthy;
    light_mode_t current_light_mode;
    
    // Chemical System
    float acid_remaining_ml;
    float chlorinator_runtime_hours;
    uint64_t last_acid_addition_time;
    uint64_t last_ph_correction_time;
    
    // Learning System
    float last_ph_correction_perc;
    float last_ph_at_pump_stop;
    ph_correction_history_t ph_history[10];
    int ph_history_index;
    orp_adjustment_history_t orp_history[10];
    int orp_history_index;
    
    // Timing
    uint64_t pump_start_time;
    uint64_t pump_status_change_time;
    uint64_t chlorinator_start_time;
    uint64_t duty_cycle_start_time;
    uint64_t light_change_time;
    
    // Safety
    bool emergency_stop_active;

    // Daily Dosing Tracking
    float daily_acid_dosed_ml;
    uint32_t daily_acid_reset_day;  // Day of year for reset tracking

    // Alarms
    bool pump_alarm_active;
    bool chlorinator_alarm_active;
    bool acid_low_alarm_active;
    bool sensors_healthy;
    
    // Duty Cycle Control
    bool duty_cycle_active;
    uint64_t duty_on_time_ms;
    
    // Light Control
    bool light_mode_change_pending;
    uint64_t light_mode_change_time;
    light_mode_t pending_light_mode;
    
    // Learning Evaluation
    bool ph_learning_evaluation_pending;
    uint64_t ph_learning_evaluation_time;
    bool orp_learning_evaluation_pending;
    uint64_t orp_learning_evaluation_time;
} system_state_t;

// Global Variables
extern system_config_t g_config;
extern system_state_t g_state;
extern const light_timing_t light_timings[];

// Function Declarations

// Main System
void pool_controller_init(void);
void pool_controller_task(void *pvParameters);

// Atlas Scientific
esp_err_t atlas_scientific_init(void);
esp_err_t atlas_send_command(uint8_t device_addr, const char* command, char* response, size_t response_size);
float atlas_read_ph_orp_ph(void);
float atlas_read_ph_orp_orp(void);
float atlas_read_ph(void);
float atlas_read_orp(void);
float atlas_read_temperature(void);
esp_err_t atlas_dose_acid(float volume_ml);
esp_err_t atlas_stop_dosing(void);
esp_err_t atlas_get_pump_status(char* status, size_t status_size);
esp_err_t atlas_calibrate_ph(float buffer_value);
esp_err_t atlas_calibrate_orp(float standard_mv);
esp_err_t atlas_clear_ph_calibration(void);
esp_err_t atlas_clear_orp_calibration(void);
esp_err_t atlas_factory_reset(uint8_t device_addr);
esp_err_t atlas_find_device(uint8_t device_addr);
esp_err_t atlas_get_device_info(uint8_t device_addr, char* info, size_t info_size);
esp_err_t atlas_sleep_device(uint8_t device_addr);
bool atlas_is_device_ready(uint8_t device_addr);
esp_err_t atlas_calibrate_pump_volume(float target_volume_ml);
esp_err_t atlas_set_pump_calibration(float actual_volume_ml);
esp_err_t atlas_clear_pump_calibration(void);
esp_err_t atlas_get_pump_calibration_status(char* status, size_t status_size);
esp_err_t atlas_set_pump_max_volume(float max_volume_ml);
esp_err_t atlas_get_pump_total_volume(float* total_ml);
esp_err_t atlas_clear_pump_total_volume(void);
void atlas_maintenance_routine(void);

// Shelly Control
esp_err_t shelly_control_init(void);
esp_err_t shelly_queue_switch(uint8_t channel, bool on);
void shelly_get_cached_state(shelly_channel_status_t *pump_status, shelly_channel_status_t *chlorinator_status, bool *reachable);
bool shelly_is_reachable(void);

// Pump Control
void pump_control_init(void);
void update_pump_status(void);
pump_status_t detect_pump_status(void);
bool is_pump_healthy(void);
void set_pump_relay(bool state);
const char* get_pump_status_string(pump_status_t status);

// Chlorinator Control
void chlorinator_control_init(void);
void manage_duty_cycle(void);
bool chlorinator_interlock_ok(void);
const char* get_interlock_failure_reason(void);
void set_chlorinator_relay(bool state);

// Light Control
void light_control_init(void);
void set_light_mode(light_mode_t mode);
void schedule_light_mode_change(light_mode_t new_mode);
void check_light_mode_change(void);
const char* get_light_mode_name(light_mode_t mode);

// Chemical Dosing
void chemical_dosing_init(void);
void perform_base_acid_addition(void);
void perform_ph_correction(void);
void track_chlorinator_runtime(void);
float calculate_expected_ph_change(float acid_ml);
void refill_acid_bottle(float new_volume_ml);
void manual_acid_dose(float volume_ml);
float get_daily_acid_consumption(void);
uint32_t get_acid_days_remaining(void);

// Learning Systems
void learning_systems_init(void);
void update_ph_learning_gain(float effectiveness_ratio);
void update_orp_learning_gain(float effectiveness_ratio);
void evaluate_ph_correction_effectiveness(void);
void evaluate_orp_adjustment_effectiveness(void);
void adjust_chlorinator_for_orp(void);
void update_base_acid_learning(void);
float get_learning_effectiveness_ph(void);
float get_learning_effectiveness_orp(void);

// Web Server
esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
void web_server_send_realtime_data(void);

// HMI Communication
esp_err_t hmi_communication_init(void);
void hmi_send_data(void);
void hmi_process_commands(void);

// System Configuration
esp_err_t system_config_init(void);
esp_err_t system_config_save(void);
esp_err_t system_config_load(void);
void system_config_set_defaults(void);
esp_err_t system_config_save_runtime_state(void);
esp_err_t system_config_load_runtime_state(void);
esp_err_t system_config_save_learning_history(void);
esp_err_t system_config_load_learning_history(void);
esp_err_t system_config_reset_to_defaults(void);
esp_err_t system_config_update_pump_thresholds(float stopped_max, float low_min, float medium_min, float high_min);
esp_err_t system_config_update_ph_settings(float target, float base_amount, float learning_gain);
esp_err_t system_config_update_orp_settings(float target, float temp_coeff, float learning_gain, uint32_t interval_ms);
esp_err_t system_config_update_chlorinator_settings(float duty_cycle, uint32_t period_ms);
esp_err_t system_config_update_wifi_settings(const char* ssid, const char* password);
esp_err_t system_config_update_shelly_settings(const char* ip, const char* user, const char* password);
void system_config_print_current(void);

// NTP Sync
esp_err_t ntp_sync_init(void);
void ntp_sync_time(void);
uint64_t get_timestamp_ms(void);

// WiFi Manager
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(void);
bool wifi_manager_is_connected(void);
const char* wifi_manager_get_status_string(void);
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t* ip_info);
void wifi_manager_get_ip_string(char* ip_str, size_t max_len);
int wifi_manager_get_rssi(void);
const char* wifi_manager_get_rssi_description(int rssi);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_reconnect(void);
void wifi_manager_status_report(void);
void wifi_manager_maintenance_task(void);

// API Authentication
bool web_server_verify_auth(httpd_req_t *req);
esp_err_t web_server_generate_token(const char* password, char* token_out, size_t token_size);

// Utility Functions
float calculate_temperature_compensated_orp(float water_temp);
float calculate_time_based_orp_target(int current_hour);
float calculate_optimal_orp_target(void);
void log_event(const char* message);
void raise_alarm(const char* alarm_id, const char* message);
void clear_alarm(const char* alarm_id);
void emergency_stop_activate(void);
void emergency_stop_reset(void);

#endif // POOL_CONTROLLER_H
