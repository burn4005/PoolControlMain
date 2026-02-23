#ifndef HMI_MAIN_H
#define HMI_MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

// Waveshare ESP32-S3-Touch-LCD-7 panel and buses
#define LCD_H_RES 800
#define LCD_V_RES 480
#define LVGL_TICK_PERIOD_MS 2

#define LCD_PIN_NUM_HSYNC GPIO_NUM_46
#define LCD_PIN_NUM_VSYNC GPIO_NUM_3
#define LCD_PIN_NUM_DE GPIO_NUM_5
#define LCD_PIN_NUM_PCLK GPIO_NUM_7

#define LCD_PIN_NUM_D0 GPIO_NUM_14
#define LCD_PIN_NUM_D1 GPIO_NUM_38
#define LCD_PIN_NUM_D2 GPIO_NUM_18
#define LCD_PIN_NUM_D3 GPIO_NUM_17
#define LCD_PIN_NUM_D4 GPIO_NUM_10
#define LCD_PIN_NUM_D5 GPIO_NUM_39
#define LCD_PIN_NUM_D6 GPIO_NUM_0
#define LCD_PIN_NUM_D7 GPIO_NUM_45
#define LCD_PIN_NUM_D8 GPIO_NUM_48
#define LCD_PIN_NUM_D9 GPIO_NUM_47
#define LCD_PIN_NUM_D10 GPIO_NUM_21
#define LCD_PIN_NUM_D11 GPIO_NUM_1
#define LCD_PIN_NUM_D12 GPIO_NUM_2
#define LCD_PIN_NUM_D13 GPIO_NUM_42
#define LCD_PIN_NUM_D14 GPIO_NUM_41
#define LCD_PIN_NUM_D15 GPIO_NUM_40

#define LCD_PIXEL_CLOCK_HZ (12900000)

#define I2C_PORT_NUM I2C_NUM_0
#define I2C_FREQ_HZ 400000
#define I2C_PIN_NUM_SDA GPIO_NUM_8
#define I2C_PIN_NUM_SCL GPIO_NUM_9
#define TOUCH_PIN_NUM_INT GPIO_NUM_4

#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 115200
#define UART_TX_PIN 15
#define UART_RX_PIN 16
#define UART_BUF_SIZE 1024

typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_MANUAL,
    SCREEN_LIGHTING,
    SCREEN_SETTINGS,
    SCREEN_ALARMS,
    SCREEN_CALIBRATION,
    SCREEN_SENSOR_CALIBRATION,
    SCREEN_DATA,
    SCREEN_COUNT
} screen_id_t;

typedef struct {
    float temperature;
    float ph;
    float orp;
    float pump_current;
    float chlorinator_current;

    bool pump_relay_on;
    bool chlorinator_relay_on;
    bool light_relay_on;
    int pump_status;
    bool pump_healthy;
    int current_light_mode;

    float acid_remaining_ml;
    float chlorinator_runtime_hours;

    float ph_correction_learning_gain;
    float orp_learning_gain;
    float base_acid_rate;

    float ph_target;
    float orp_target;
    float chlorinator_duty_cycle;
    float pool_volume_liters;
    float hcl_concentration_percent;

    bool pump_alarm_active;
    bool chlorinator_alarm_active;
    bool acid_low_alarm_active;
    bool sensors_healthy;

    uint64_t timestamp;
} system_data_t;

typedef struct {
    char command[32];
    char parameter[32];
    float value;
    bool bool_value;
    char string_value[64];
} hmi_command_t;

typedef void *esp_lcd_touch_handle_t;

extern system_data_t g_system_data;
extern system_data_t g_system_data_view;
extern lv_disp_t *g_disp;
extern bool g_data_updated;
extern uint64_t g_last_data_rx_ms;
extern esp_lcd_touch_handle_t g_touch_handle;
extern SemaphoreHandle_t g_system_data_mutex;

void hmi_init(void);
void hmi_task(void *pvParameters);

void gui_manager_init(void);
void gui_manager_update_data(void);
void gui_manager_periodic(void);
void gui_manager_switch_screen(screen_id_t screen_id);
screen_id_t gui_manager_get_current_screen(void);

void screen_dashboard_create(void);
void screen_dashboard_update(void);
lv_obj_t *get_dashboard_screen(void);

void screen_manual_create(void);
void screen_manual_update(void);
lv_obj_t *get_manual_screen(void);

void screen_lighting_create(void);
void screen_lighting_update(void);
lv_obj_t *get_lighting_screen(void);

void screen_settings_create(void);
void screen_settings_update(void);
lv_obj_t *get_settings_screen(void);

void screen_alarms_create(void);
void screen_alarms_update(void);
lv_obj_t *get_alarms_screen(void);

void screen_calibration_create(void);
void screen_calibration_update(void);
lv_obj_t *get_calibration_screen(void);

void screen_sensor_calibration_create(void);
void screen_sensor_calibration_update(void);
lv_obj_t *get_sensor_calibration_screen(void);

void screen_data_create(void);
void screen_data_update(void);
lv_obj_t *get_data_screen(void);

esp_err_t communication_init(void);
void communication_task(void *pvParameters);
esp_err_t send_command(const char *command, const char *parameter, float value);
esp_err_t send_command_bool(const char *command, const char *parameter, bool value);
esp_err_t send_command_string(const char *command, const char *parameter, const char *string_value);

void touch_handler_init(void);
void touch_handler_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

const char *get_pump_status_string(int status);
const char *get_light_mode_string(int mode);
lv_color_t get_status_color(bool healthy);
void format_time_string(char *buffer, size_t size, uint64_t timestamp_ms);
void format_volume_string(char *buffer, size_t size, float volume_ml);

lv_obj_t *create_status_card(lv_obj_t *parent, const char *title, int x, int y, int width, int height);
lv_obj_t *create_button_with_label(lv_obj_t *parent, const char *text, int x, int y, int width, int height);
lv_obj_t *create_value_display(lv_obj_t *parent, const char *label, const char *value, const char *unit, int x, int y);
void update_value_display(lv_obj_t *value_obj, float value, const char *format, const char *unit);

#endif
