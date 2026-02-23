#ifndef CH422G_H
#define CH422G_H

#include "driver/i2c.h"
#include "esp_err.h"

// CH422G I2C addresses (7-bit)
#define CH422G_ADDR_SET_IO    0x24  // Set I/O direction and mode
#define CH422G_ADDR_WR_OC     0x23  // Write OC outputs (active low)
#define CH422G_ADDR_WR_IO     0x38  // Write IO outputs
#define CH422G_ADDR_RD_IO     0x26  // Read IO inputs

// EXIO pin definitions for Waveshare ESP32-S3-Touch-LCD-7
#define CH422G_EXIO_TOUCH_RST  1   // EXIO1 - Touch reset
#define CH422G_EXIO_BACKLIGHT  2   // EXIO2 - LCD backlight

esp_err_t ch422g_init(i2c_port_t i2c_port);
esp_err_t ch422g_set_io_output(i2c_port_t i2c_port);
esp_err_t ch422g_write_output(i2c_port_t i2c_port, uint8_t pin_mask);
esp_err_t ch422g_set_pin(i2c_port_t i2c_port, uint8_t pin, bool level);

#endif // CH422G_H
