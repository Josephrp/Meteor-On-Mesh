/*
 * TCA9548A I2C multiplexer driver (ESP-IDF)
 */

#include "tca9548a.h"
#include <esp_log.h>

static const char *TAG = "tca9548a";

esp_err_t tca9548a_init_desc(tca9548a_t *dev, uint8_t addr, i2c_port_t port,
                             gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->selected = 0;

    dev->dev.port = port;
    dev->dev.addr = addr;
    dev->dev.cfg.sda_io_num = sda_gpio;
    dev->dev.cfg.scl_io_num = scl_gpio;
    dev->dev.cfg.master.clk_speed = 400000; // safe for TCA + most sensors

    esp_err_t err = i2c_dev_create_mutex(&dev->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mutex create failed");
        return err;
    }
    // Probe by writing 0
    err = tca9548a_select(dev, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TCA9548A not responding at 0x%02x", addr);
    }
    return ESP_OK;
}

esp_err_t tca9548a_free_desc(tca9548a_t *dev) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_dev_delete_mutex(&dev->dev);
}

esp_err_t tca9548a_select(tca9548a_t *dev, uint8_t mask) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint8_t buf = mask;
    esp_err_t err = i2c_dev_write(&dev->dev, NULL, 0, &buf, 1);
    if (err == ESP_OK) {
        dev->selected = mask;
    }
    return err;
}

esp_err_t tca9548a_select_channel(tca9548a_t *dev, uint8_t ch) {
    if (ch > 7) return ESP_ERR_INVALID_ARG;
    return tca9548a_select(dev, (uint8_t)(1u << ch));
}

esp_err_t tca9548a_get_selected(tca9548a_t *dev, uint8_t *mask) {
    if (!dev || !mask) return ESP_ERR_INVALID_ARG;
    uint8_t buf = 0;
    esp_err_t err = i2c_dev_read(&dev->dev, NULL, 0, &buf, 1);
    if (err == ESP_OK) {
        *mask = buf;
        dev->selected = buf;
    }
    return err;
}
