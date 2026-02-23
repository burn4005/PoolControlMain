#include "hmi_main.h"

#include "ch422g.h"
#include "esp_heap_caps.h"
#include "theme_pool.h"
#include "trend_data.h"

static const char *TAG = "HMI_MAIN";

static lv_disp_draw_buf_t s_disp_buf;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;
static esp_lcd_panel_handle_t s_panel_handle;

esp_lcd_touch_handle_t g_touch_handle = NULL;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void init_i2c(void)
{
    i2c_config_t i2c_conf = {};
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = I2C_PIN_NUM_SDA;
    i2c_conf.scl_io_num = I2C_PIN_NUM_SCL;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = I2C_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT_NUM, i2c_conf.mode, 0, 0, 0));
}

static void init_rgb_panel(void)
{
    const int data_pins[16] = {
        LCD_PIN_NUM_D0, LCD_PIN_NUM_D1, LCD_PIN_NUM_D2, LCD_PIN_NUM_D3,
        LCD_PIN_NUM_D4, LCD_PIN_NUM_D5, LCD_PIN_NUM_D6, LCD_PIN_NUM_D7,
        LCD_PIN_NUM_D8, LCD_PIN_NUM_D9, LCD_PIN_NUM_D10, LCD_PIN_NUM_D11,
        LCD_PIN_NUM_D12, LCD_PIN_NUM_D13, LCD_PIN_NUM_D14, LCD_PIN_NUM_D15,
    };

    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.data_width = 16;
    panel_config.bits_per_pixel = 16;
    panel_config.num_fbs = 2;
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.disp_gpio_num = GPIO_NUM_NC;
    panel_config.pclk_gpio_num = LCD_PIN_NUM_PCLK;
    panel_config.vsync_gpio_num = LCD_PIN_NUM_VSYNC;
    panel_config.hsync_gpio_num = LCD_PIN_NUM_HSYNC;
    panel_config.de_gpio_num = LCD_PIN_NUM_DE;
    memcpy(panel_config.data_gpio_nums, data_pins, sizeof(data_pins));

    panel_config.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    panel_config.timings.h_res = LCD_H_RES;
    panel_config.timings.v_res = LCD_V_RES;
    panel_config.timings.hsync_back_porch = 4;
    panel_config.timings.hsync_front_porch = 4;
    panel_config.timings.hsync_pulse_width = 2;
    panel_config.timings.vsync_back_porch = 4;
    panel_config.timings.vsync_front_porch = 4;
    panel_config.timings.vsync_pulse_width = 2;

    panel_config.flags.fb_in_psram = 1;

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
}

void hmi_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI for Waveshare RGB panel");

    lv_init();

    if (g_system_data_mutex == NULL) {
        g_system_data_mutex = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(g_system_data_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    }
    g_system_data_view = g_system_data;

    init_i2c();
    ESP_ERROR_CHECK(ch422g_init(I2C_PORT_NUM));

    // Hold and release GT911 reset via CH422G EXIO1.
    ESP_ERROR_CHECK(ch422g_set_pin(I2C_PORT_NUM, CH422G_EXIO_TOUCH_RST, false));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(ch422g_set_pin(I2C_PORT_NUM, CH422G_EXIO_TOUCH_RST, true));

    // Enable LCD backlight via CH422G EXIO2.
    ESP_ERROR_CHECK(ch422g_set_pin(I2C_PORT_NUM, CH422G_EXIO_BACKLIGHT, true));

    init_rgb_panel();

    const size_t draw_lines = 48;
    const size_t px_count = LCD_H_RES * draw_lines;
    s_buf1 = static_cast<lv_color_t *>(heap_caps_malloc(px_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_buf2 = static_cast<lv_color_t *>(heap_caps_malloc(px_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ESP_ERROR_CHECK((s_buf1 != NULL && s_buf2 != NULL) ? ESP_OK : ESP_ERR_NO_MEM);

    lv_disp_draw_buf_init(&s_disp_buf, s_buf1, s_buf2, px_count);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &s_disp_buf;
    disp_drv.user_data = s_panel_handle;
    g_disp = lv_disp_drv_register(&disp_drv);

    touch_handler_init();
    ESP_ERROR_CHECK(communication_init());
    ESP_ERROR_CHECK(trend_data_init());

    theme_pool_init(g_disp);
    gui_manager_init();

    ESP_LOGI(TAG, "HMI initialization complete");
}

void hmi_task(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t period = pdMS_TO_TICKS(10);
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        lv_timer_handler();
        bool should_update = false;
        if (xSemaphoreTake(g_system_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (g_data_updated) {
                should_update = true;
                g_data_updated = false;
                g_system_data_view = g_system_data;
            }
            xSemaphoreGive(g_system_data_mutex);
        }
        if (should_update) {
            gui_manager_update_data();
        }
        gui_manager_periodic();

        vTaskDelayUntil(&last_wake, period);
    }
}

const char *get_pump_status_string(int status)
{
    switch (status) {
        case 0: return "OFF";
        case 1: return "LOW";
        case 2: return "MED";
        case 3: return "HIGH";
        case 4: return "STOPPED";
        default: return "UNKNOWN";
    }
}

const char *get_light_mode_string(int mode)
{
    static const char *modes[] = {
        "Off", "Blue", "Pink", "Red", "Yellow", "Green", "Cyan", "White", "Mode 1", "Mode 2", "Mode 3", "Mode 4", "Brightness"
    };
    if (mode >= 0 && mode < 13) {
        return modes[mode];
    }
    return "Unknown";
}

lv_color_t get_status_color(bool healthy)
{
    return healthy ? lv_color_hex(0x00E676) : lv_color_hex(0xFF1744);
}

void format_time_string(char *buffer, size_t size, uint64_t timestamp_ms)
{
    uint64_t sec = timestamp_ms / 1000;
    uint64_t min = sec / 60;
    uint64_t hrs = min / 60;
    if (hrs > 0) {
        snprintf(buffer, size, "%lluh %llum", hrs, min % 60);
    } else if (min > 0) {
        snprintf(buffer, size, "%llum %llus", min, sec % 60);
    } else {
        snprintf(buffer, size, "%llus", sec);
    }
}

void format_volume_string(char *buffer, size_t size, float volume_ml)
{
    if (volume_ml >= 1000.0f) {
        snprintf(buffer, size, "%.2fL", volume_ml / 1000.0f);
    } else {
        snprintf(buffer, size, "%.0fml", volume_ml);
    }
}

lv_obj_t *create_status_card(lv_obj_t *parent, const char *title, int x, int y, int width, int height)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x22335A), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 16, 0);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

lv_obj_t *create_button_with_label(lv_obj_t *parent, const char *text, int x, int y, int width, int height)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, width, height);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x24365D), 0);
    lv_obj_set_style_radius(btn, 14, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *create_value_display(lv_obj_t *parent, const char *label, const char *value, const char *unit, int x, int y)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, 170, 74);
    lv_obj_set_pos(wrap, x, y);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);

    lv_obj_t *lbl = lv_label_create(wrap);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val = lv_label_create(wrap);
    char text[40];
    snprintf(text, sizeof(text), "%s %s", value, unit);
    lv_label_set_text(val, text);
    lv_obj_set_style_text_color(val, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return val;
}

void update_value_display(lv_obj_t *value_obj, float value, const char *format, const char *unit)
{
    char text[40];
    snprintf(text, sizeof(text), format, value);
    size_t len = strlen(text);
    snprintf(text + len, sizeof(text) - len, " %s", unit);
    lv_label_set_text(value_obj, text);
}


