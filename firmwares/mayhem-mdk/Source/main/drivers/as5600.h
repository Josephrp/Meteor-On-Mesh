/*
 * Basic AS5600 magnetic angle sensor driver (ESP-IDF)
 * Used on Meshtonic H4M for rotary / magnetic position.
 */
#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "i2cdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AS5600_ADDR 0x36

typedef struct {
    i2c_dev_t dev;
    bool ready;
} as5600_t;

esp_err_t as5600_init_desc(as5600_t *dev, uint8_t addr, i2c_port_t port,
                           gpio_num_t sda_gpio, gpio_num_t scl_gpio);

esp_err_t as5600_free_desc(as5600_t *dev);

/** Read 12-bit raw angle (0..4095). Returns ESP_OK + value. */
esp_err_t as5600_read_raw_angle(as5600_t *dev, uint16_t *angle12);

#ifdef __cplusplus
}
#endif
