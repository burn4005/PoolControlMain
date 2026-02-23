#include "hmi_main.h"

static const char *TAG = "HMI_MAIN";

// LVGL display buffer
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf_1[LCD_H_RES * 20];
static lv_color_t buf_2[LCD_H_RES * 20];

// LVGL input device
static lv_indev_t *indev_touchpad;

// LCD and touch handles
static esp_lcd_panel_handle_t panel_handle = NULL;

// LVGL tick is handled automatically via LV_TICK_CUSTOM (esp_timer_get_time)

// LVGL flush callback
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

// LVGL display flush
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

void hmi_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI...");
    
    // Initialize LCD
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .dc_gpio_num = PIN_NUM_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = NULL, // Will be set later
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    
    ESP_LOGI(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    // Configure backlight
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
    
    // Initialize LVGL
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    
    // Allocate draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf_1, buf_2, LCD_H_RES * 20);
    
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    g_disp = lv_disp_drv_register(&disp_drv);
    
    // Update the IO config with the display driver
    io_config.user_ctx = &disp_drv;
    
    // Initialize touch
    touch_handler_init();
    
    // Initialize communication
    communication_init();
    
    // Initialize GUI
    gui_manager_init();
    
    ESP_LOGI(TAG, "HMI Initialization Complete");
}

void hmi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HMI Task Started");
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms for smooth GUI
    
    while (1) {
        // Handle LVGL tasks
        lv_timer_handler();
        
        // Update GUI if new data received
        if (g_data_updated) {
            gui_manager_update_data();
            g_data_updated = false;
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Utility Functions
const char* get_pump_status_string(int status)
{
    switch (status) {
        case 0: return "OFF";
        case 1: return "LOW SPEED";
        case 2: return "MEDIUM SPEED";
        case 3: return "HIGH SPEED";
        case 4: return "ON BUT STOPPED";
        default: return "UNKNOWN";
    }
}

const char* get_light_mode_string(int mode)
{
    const char* modes[] = {
        "Off", "Blue", "Pink", "Red", "Yellow", 
        "Green", "Cyan", "White", "Mode 1", "Mode 2", 
        "Mode 3", "Mode 4", "Brightness"
    };
    
    if (mode >= 0 && mode < 13) {
        return modes[mode];
    }
    return "Unknown";
}

lv_color_t get_status_color(bool healthy)
{
    return healthy ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
}

void format_time_string(char* buffer, size_t size, uint64_t timestamp_ms)
{
    uint64_t seconds = timestamp_ms / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    if (hours > 0) {
        snprintf(buffer, size, "%lluh %llum", hours, minutes % 60);
    } else if (minutes > 0) {
        snprintf(buffer, size, "%llum %llus", minutes, seconds % 60);
    } else {
        snprintf(buffer, size, "%llus", seconds);
    }
}

void format_volume_string(char* buffer, size_t size, float volume_ml)
{
    if (volume_ml >= 1000.0f) {
        snprintf(buffer, size, "%.1fL", volume_ml / 1000.0f);
    } else {
        snprintf(buffer, size, "%.0fml", volume_ml);
    }
}

// LVGL Helper Functions
lv_obj_t* create_status_card(lv_obj_t* parent, const char* title, int x, int y, int width, int height)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    
    // Title label
    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x333333), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    
    return card;
}

lv_obj_t* create_button_with_label(lv_obj_t* parent, const char* text, int x, int y, int width, int height)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 8, 0);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t* create_value_display(lv_obj_t* parent, const char* label, const char* value, const char* unit, int x, int y)
{
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, 120, 60);
    lv_obj_set_pos(container, x, y);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(container, LV_OPA_TRANSP, 0);
    
    // Label
    lv_obj_t* label_obj = lv_label_create(container);
    lv_label_set_text(label_obj, label);
    lv_obj_set_style_text_font(label_obj, &lv_font_montserrat_12, 0);
    lv_obj_align(label_obj, LV_ALIGN_TOP_MID, 0, 0);
    
    // Value
    lv_obj_t* value_obj = lv_label_create(container);
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%s %s", value, unit);
    lv_label_set_text(value_obj, value_text);
    lv_obj_set_style_text_font(value_obj, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(value_obj, lv_color_hex(0x0066CC), 0);
    lv_obj_align(value_obj, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    return value_obj;
}

void update_value_display(lv_obj_t* value_obj, float value, const char* format, const char* unit)
{
    char value_text[32];
    snprintf(value_text, sizeof(value_text), format, value);
    strcat(value_text, " ");
    strcat(value_text, unit);
    lv_label_set_text(value_obj, value_text);
}
