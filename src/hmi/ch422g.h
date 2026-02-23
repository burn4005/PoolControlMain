#ifndef CH422G_H
#define CH422G_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

#define CH422G_I2C_ADDR_SET_IO 0x24
#define CH422G_I2C_ADDR_WR_OC 0x23
#define CH422G_I2C_ADDR_WR_IO 0x38
#define CH422G_EXIO_TOUCH_RST 1
#define CH422G_EXIO_BACKLIGHT 2

esp_err_t ch422g_init(i2c_port_t i2c_port);
esp_err_t ch422g_set_pin(i2c_port_t i2c_port, uint8_t pin, bool level);

#endif
