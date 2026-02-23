#include "hmi_main.h"

static const char *TAG = "TOUCH_HANDLER";

static const uint8_t GT911_I2C_ADDR = 0x5D;
static const uint16_t GT911_REG_STATUS = 0x814E;
static const uint16_t GT911_REG_POINT1 = 0x8150;

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t addr[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    return i2c_master_write_read_device(I2C_PORT_NUM, GT911_I2C_ADDR, addr, sizeof(addr), data, len, pdMS_TO_TICKS(30));
}

static esp_err_t gt911_write_reg(uint16_t reg, uint8_t value)
{
    uint8_t buf[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value};
    return i2c_master_write_to_device(I2C_PORT_NUM, GT911_I2C_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(30));
}

void touch_handler_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << TOUCH_PIN_NUM_INT;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_handler_read;
    lv_indev_drv_register(&indev_drv);

    g_touch_handle = (void *)0x1;
    ESP_LOGI(TAG, "GT911 touch initialized (direct I2C)");
}

void touch_handler_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;

    if (g_touch_handle == NULL) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint8_t status = 0;
    if (gt911_read_reg(GT911_REG_STATUS, &status, 1) != ESP_OK) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    const uint8_t touch_count = status & 0x0F;
    const bool data_ready = (status & 0x80U) != 0;

    if (!data_ready || touch_count == 0) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint8_t point[8] = {0};
    if (gt911_read_reg(GT911_REG_POINT1, point, sizeof(point)) != ESP_OK) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t x = (uint16_t)point[1] | ((uint16_t)point[2] << 8);
    uint16_t y = (uint16_t)point[3] | ((uint16_t)point[4] << 8);

    if (x >= LCD_H_RES) {
        x = LCD_H_RES - 1;
    }
    if (y >= LCD_V_RES) {
        y = LCD_V_RES - 1;
    }

    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = LV_INDEV_STATE_PR;

    (void)gt911_write_reg(GT911_REG_STATUS, 0);
}
