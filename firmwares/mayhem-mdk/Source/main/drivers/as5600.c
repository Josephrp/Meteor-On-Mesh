/*
 * Basic AS5600 driver
 */
#include "as5600.h"
#include <esp_log.h>

static const char *TAG = "as5600";

#define AS5600_REG_RAW_ANGLE 0x0C

esp_err_t as5600_init_desc(as5600_t *dev, uint8_t addr, i2c_port_t port,
                           gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->ready = false;
    dev->dev.port = port;
    dev->dev.addr = addr;
    dev->dev.cfg.sda_io_num = sda_gpio;
    dev->dev.cfg.scl_io_num = scl_gpio;
    dev->dev.cfg.master.clk_speed = 400000;

    esp_err_t err = i2c_dev_create_mutex(&dev->dev);
    if (err != ESP_OK) return err;

    // Probe
    uint8_t dummy;
    err = i2c_dev_read_reg(&dev->dev, 0x00, &dummy, 1);
    if (err == ESP_OK) dev->ready = true;
    return err;
}

esp_err_t as5600_free_desc(as5600_t *dev) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_dev_delete_mutex(&dev->dev);
}

esp_err_t as5600_read_raw_angle(as5600_t *dev, uint16_t *angle12) {
    if (!dev || !angle12 || !dev->ready) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_dev_read_reg(&dev->dev, AS5600_REG_RAW_ANGLE, buf, 2);
    if (err == ESP_OK) {
        *angle12 = ((uint16_t)buf[0] << 8 | buf[1]) & 0x0FFF;
    }
    return err;
}
