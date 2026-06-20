/*
 * TCA9548A I2C multiplexer driver (ESP-IDF)
 * For Meshtonic H4M: route sensors behind the mux.
 */

#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "i2cdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCA9548A_ADDR_BASE 0x70

typedef struct {
    i2c_dev_t dev;
    uint8_t selected; // last written mask
} tca9548a_t;

/**
 * Initialize descriptor.
 * @param dev descriptor
 * @param addr full 7-bit addr (0x70 + A2A1A0)
 * @param port I2C port
 * @param sda_gpio, scl_gpio
 */
esp_err_t tca9548a_init_desc(tca9548a_t *dev, uint8_t addr, i2c_port_t port,
                             gpio_num_t sda_gpio, gpio_num_t scl_gpio);

esp_err_t tca9548a_free_desc(tca9548a_t *dev);

/**
 * Select channels (mask: bit 0 = ch0 ... bit 7 = ch7). 0 deselects all.
 */
esp_err_t tca9548a_select(tca9548a_t *dev, uint8_t mask);

/**
 * Convenience: select single channel 0-7.
 */
esp_err_t tca9548a_select_channel(tca9548a_t *dev, uint8_t ch);

/**
 * Read current selected mask from device.
 */
esp_err_t tca9548a_get_selected(tca9548a_t *dev, uint8_t *mask);

#ifdef __cplusplus
}
#endif
