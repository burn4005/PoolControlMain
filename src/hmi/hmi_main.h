#ifndef HMI_MAIN_H
#define HMI_MAIN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lvgl.h"

// Hardware Configuration for Core Electronics ESP32-S3 7" Display
#define LCD_HOST            SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_MOSI        11
#define PIN_NUM_CLK         12
#define PIN_NUM_CS          10
#define PIN_NUM_DC          13
#define PIN_NUM_RST         14
#define PIN_NUM_BK_LIGHT    15

// Touch Configuration
#define TOUCH_HOST          SPI3_HOST
#define PIN_NUM_TOUCH_MOSI  6
#define PIN_NUM_TOUCH_CLK   7
#define PIN_NUM_TOUCH_CS    5
#define PIN_NUM_TOUCH_INT   4

// UART Configuration for Main Controller Communication
#define UART_PORT_NUM       UART_NUM_1
#define UART_BAUD_RATE      115200
#define UART_TX_PIN         17
#define UART_RX_PIN         16
#define UART_BUF_SIZE       1024

// Display Configuration
#define LCD_H_RES           800
#define LCD_V_RES           480
#define LVGL_TICK_PERIOD_MS 2

// Screen IDs
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_MANUAL = 1,
    SCREEN_LIGHTING = 2,
    SCREEN_SETTINGS = 3,
    SCREEN_ALARMS = 4,
    SCREEN_CALIBRATION = 5,
    SCREEN_DATA = 6,
    SCREEN_COUNT
} screen_id_t;

// System Data Structure (received from main controller)
typedef struct {
    // Sensor Readings
    float temperature;
    float ph;
    float orp;
    float pump_current;
    float chlorinator_current;
    
    // Equipment Status
    bool pump_relay_on;
    bool chlorinator_relay_on;
    bool light_relay_on;
    int pump_status;  // 0=OFF, 1=LOW, 2=MED, 3=HIGH, 4=ON_BUT_STOPPED
    bool pump_healthy;
    int current_light_mode;
    
    // Chemical System
    float acid_remaining_ml;
    float chlorinator_runtime_hours;
    
    // Learning System
    float ph_correction_learning_gain;
    float orp_learning_gain;
    float base_acid_rate;
    
    // Configuration
    float ph_target;
    float orp_target;
    float chlorinator_duty_cycle;
    float pool_volume_liters;
    float hcl_concentration_percent;
    
    // Alarms
    bool pump_alarm_active;
    bool chlorinator_alarm_active;
    bool acid_low_alarm_active;
    bool sensors_healthy;
    
    // Timestamps
    uint64_t timestamp;
} system_data_t;

// Command Structure (sent to main controller)
typedef struct {
    char command[32];
    char parameter[32];
    float value;
    bool bool_value;
    char string_value[64];
} hmi_command_t;

// Global Variables
extern system_data_t g_system_data;
extern lv_disp_t *g_disp;
extern bool g_data_updated;

// Function Declarations

// Main HMI
void hmi_init(void);
void hmi_task(void *pvParameters);

// GUI Manager
void gui_manager_init(void);
void gui_manager_update_data(void);
void gui_manager_switch_screen(screen_id_t screen_id);
screen_id_t gui_manager_get_current_screen(void);

// Screen Functions
void screen_dashboard_create(void);
void screen_dashboard_update(void);
void screen_manual_create(void);
void screen_manual_update(void);
void screen_lighting_create(void);
void screen_lighting_update(void);
void screen_settings_create(void);
void screen_settings_update(void);
void screen_alarms_create(void);
void screen_alarms_update(void);
void screen_calibration_create(void);
void screen_calibration_update(void);
void screen_data_create(void);
void screen_data_update(void);

// Communication
esp_err_t communication_init(void);
void communication_task(void *pvParameters);
esp_err_t send_command(const char* command, const char* parameter, float value);
esp_err_t send_command_bool(const char* command, const char* parameter, bool value);
esp_err_t send_command_string(const char* command, const char* parameter, const char* string_value);

// Touch Handler
void touch_handler_init(void);
void touch_handler_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

// Utility Functions
const char* get_pump_status_string(int status);
const char* get_light_mode_string(int mode);
lv_color_t get_status_color(bool healthy);
void format_time_string(char* buffer, size_t size, uint64_t timestamp_ms);
void format_volume_string(char* buffer, size_t size, float volume_ml);

// LVGL Helpers
lv_obj_t* create_status_card(lv_obj_t* parent, const char* title, int x, int y, int width, int height);
lv_obj_t* create_button_with_label(lv_obj_t* parent, const char* text, int x, int y, int width, int height);
lv_obj_t* create_value_display(lv_obj_t* parent, const char* label, const char* value, const char* unit, int x, int y);
void update_value_display(lv_obj_t* value_obj, float value, const char* format, const char* unit);

#endif // HMI_MAIN_H
