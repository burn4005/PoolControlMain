#include "ch422g.h"

#include "esp_log.h"

static const char *TAG = "CH422G";

static uint8_t s_output_state;

static esp_err_t ch422g_write_addr(i2c_port_t i2c_port, uint8_t addr, uint8_t data)
{
    return i2c_master_write_to_device(i2c_port, addr, &data, 1, pdMS_TO_TICKS(50));
}

esp_err_t ch422g_init(i2c_port_t i2c_port)
{
    s_output_state = 0x00;
    // Configure IO mode for EXIO pins and drive known outputs.
    esp_err_t err = ch422g_write_addr(i2c_port, CH422G_I2C_ADDR_SET_IO, 0x00);
    if (err == ESP_OK) {
        err = ch422g_write_addr(i2c_port, CH422G_I2C_ADDR_WR_OC, 0x00);
    }
    if (err == ESP_OK) {
        err = ch422g_write_addr(i2c_port, CH422G_I2C_ADDR_WR_IO, s_output_state);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CH422G init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "CH422G initialized");
    return ESP_OK;
}

esp_err_t ch422g_set_pin(i2c_port_t i2c_port, uint8_t pin, bool level)
{
    if (pin > 7) {
        return ESP_ERR_INVALID_ARG;
    }

    if (level) {
        s_output_state |= (uint8_t)(1U << pin);
    } else {
        s_output_state &= (uint8_t)~(1U << pin);
    }

    esp_err_t err = ch422g_write_addr(i2c_port, CH422G_I2C_ADDR_WR_IO, s_output_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set EXIO%u=%u: %s", pin, level ? 1 : 0, esp_err_to_name(err));
    }
    return err;
}
