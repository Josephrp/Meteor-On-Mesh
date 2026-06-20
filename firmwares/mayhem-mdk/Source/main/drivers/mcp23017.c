/*
 * MCP23017 16-bit I2C GPIO expander driver (ESP-IDF)
 */

#include "mcp23017.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "mcp23017";

static inline esp_err_t mcp_write_reg(mcp23017_t *dev, uint8_t reg, uint8_t val) {
    return i2c_dev_write_reg(&dev->dev, reg, &val, 1);
}

static inline esp_err_t mcp_read_reg(mcp23017_t *dev, uint8_t reg, uint8_t *val) {
    return i2c_dev_read_reg(&dev->dev, reg, val, 1);
}

esp_err_t mcp23017_init_desc(mcp23017_t *dev, uint8_t addr, i2c_port_t port,
                             gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    memset(dev, 0, sizeof(*dev));

    dev->dev.port = port;
    dev->dev.addr = addr;
    dev->dev.cfg.sda_io_num = sda_gpio;
    dev->dev.cfg.scl_io_num = scl_gpio;
    dev->dev.cfg.master.clk_speed = 400000;

    esp_err_t err = i2c_dev_create_mutex(&dev->dev);
    if (err != ESP_OK) return err;

    // IOCON: disable sequential op (SEQOP=1), mirror interrupts off, etc.
    // BANK=0, SEQOP=1, DISSLW=0, HAEN=0 (not used), ODR=0, INTPOL=0
    err = mcp_write_reg(dev, MCP_REG_IOCON, 0x20 /* SEQOP */);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IOCON write failed");
        return err;
    }

    // Default all outputs low, all inputs (safe start)
    dev->dir = 0xFFFF;
    dev->out = 0x0000;
    if ((err = mcp_write_reg(dev, MCP_REG_IODIRA, 0xFF)) != ESP_OK) return err;
    if ((err = mcp_write_reg(dev, MCP_REG_IODIRB, 0xFF)) != ESP_OK) return err;
    if ((err = mcp_write_reg(dev, MCP_REG_OLATA, 0x00)) != ESP_OK) return err;
    if ((err = mcp_write_reg(dev, MCP_REG_OLATB, 0x00)) != ESP_OK) return err;

    return ESP_OK;
}

esp_err_t mcp23017_free_desc(mcp23017_t *dev) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_dev_delete_mutex(&dev->dev);
}

esp_err_t mcp23017_set_direction(mcp23017_t *dev, uint16_t dir_mask) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint8_t a = dir_mask & 0xFF;
    uint8_t b = (dir_mask >> 8) & 0xFF;
    esp_err_t err = mcp_write_reg(dev, MCP_REG_IODIRA, a);
    if (err == ESP_OK) err = mcp_write_reg(dev, MCP_REG_IODIRB, b);
    if (err == ESP_OK) dev->dir = dir_mask;
    return err;
}

esp_err_t mcp23017_set_pin_direction(mcp23017_t *dev, uint8_t pin, bool input) {
    if (!dev || pin > 15) return ESP_ERR_INVALID_ARG;
    uint16_t mask = (uint16_t)(1u << pin);
    uint16_t newdir = input ? (dev->dir | mask) : (dev->dir & ~mask);
    // Only write the affected port byte for efficiency
    bool portb = pin >= 8;
    uint8_t reg = portb ? MCP_REG_IODIRB : MCP_REG_IODIRA;
    uint8_t val = (newdir >> (portb ? 8 : 0)) & 0xFF;
    esp_err_t err = mcp_write_reg(dev, reg, val);
    if (err == ESP_OK) dev->dir = newdir;
    return err;
}

esp_err_t mcp23017_write_port(mcp23017_t *dev, uint16_t value) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint8_t a = value & 0xFF;
    uint8_t b = (value >> 8) & 0xFF;
    esp_err_t err = mcp_write_reg(dev, MCP_REG_OLATA, a);
    if (err == ESP_OK) err = mcp_write_reg(dev, MCP_REG_OLATB, b);
    if (err == ESP_OK) dev->out = value;
    return err;
}

esp_err_t mcp23017_write_pin(mcp23017_t *dev, uint8_t pin, bool level) {
    if (!dev || pin > 15) return ESP_ERR_INVALID_ARG;
    uint16_t mask = (uint16_t)(1u << pin);
    uint16_t newout = level ? (dev->out | mask) : (dev->out & ~mask);
    bool portb = pin >= 8;
    uint8_t reg = portb ? MCP_REG_OLATB : MCP_REG_OLATA;
    uint8_t val = (newout >> (portb ? 8 : 0)) & 0xFF;
    esp_err_t err = mcp_write_reg(dev, reg, val);
    if (err == ESP_OK) dev->out = newout;
    return err;
}

esp_err_t mcp23017_read_port(mcp23017_t *dev, uint16_t *value) {
    if (!dev || !value) return ESP_ERR_INVALID_ARG;
    uint8_t a = 0, b = 0;
    esp_err_t err = mcp_read_reg(dev, MCP_REG_GPIOA, &a);
    if (err == ESP_OK) err = mcp_read_reg(dev, MCP_REG_GPIOB, &b);
    if (err == ESP_OK) *value = ((uint16_t)b << 8) | a;
    return err;
}

esp_err_t mcp23017_read_pin(mcp23017_t *dev, uint8_t pin, bool *level) {
    if (!dev || !level || pin > 15) return ESP_ERR_INVALID_ARG;
    uint16_t port = 0;
    esp_err_t err = mcp23017_read_port(dev, &port);
    if (err == ESP_OK) {
        *level = (port & (1u << pin)) != 0;
    }
    return err;
}

esp_err_t mcp23017_set_pullup(mcp23017_t *dev, uint8_t pin, bool enable) {
    if (!dev || pin > 15) return ESP_ERR_INVALID_ARG;
    uint8_t reg = (pin >= 8) ? MCP_REG_GPPUB : MCP_REG_GPPUA;
    uint8_t cur = 0;
    esp_err_t err = mcp_read_reg(dev, reg, &cur);
    if (err != ESP_OK) return err;
    uint8_t bit = (uint8_t)(1u << (pin & 7));
    if (enable) cur |= bit; else cur &= ~bit;
    return mcp_write_reg(dev, reg, cur);
}

esp_err_t mcp23017_set_interrupt(mcp23017_t *dev, uint8_t pin, bool enable) {
    if (!dev || pin > 15) return ESP_ERR_INVALID_ARG;
    uint8_t reg = (pin >= 8) ? MCP_REG_GPINTENB : MCP_REG_GPINTENA;
    uint8_t cur = 0;
    esp_err_t err = mcp_read_reg(dev, reg, &cur);
    if (err != ESP_OK) return err;
    uint8_t bit = (uint8_t)(1u << (pin & 7));
    if (enable) cur |= bit; else cur &= ~bit;
    return mcp_write_reg(dev, reg, cur);
}

esp_err_t mcp23017_get_intf(mcp23017_t *dev, uint16_t *intf) {
    if (!dev || !intf) return ESP_ERR_INVALID_ARG;
    uint8_t a=0, b=0;
    esp_err_t err = mcp_read_reg(dev, MCP_REG_INTFA, &a);
    if (err == ESP_OK) err = mcp_read_reg(dev, MCP_REG_INTFB, &b);
    if (err == ESP_OK) *intf = ((uint16_t)b << 8) | a;
    return err;
}

esp_err_t mcp23017_get_intcap(mcp23017_t *dev, uint16_t *cap) {
    if (!dev || !cap) return ESP_ERR_INVALID_ARG;
    uint8_t a=0, b=0;
    esp_err_t err = mcp_read_reg(dev, MCP_REG_INTCAPA, &a);
    if (err == ESP_OK) err = mcp_read_reg(dev, MCP_REG_INTCAPB, &b);
    if (err == ESP_OK) *cap = ((uint16_t)b << 8) | a;
    return err;
}

esp_err_t mcp23017_read_interrupt_state(mcp23017_t *dev, uint16_t *intf, uint16_t *cap) {
    uint16_t f=0, c=0;
    esp_err_t e1 = mcp23017_get_intf(dev, &f);
    esp_err_t e2 = mcp23017_get_intcap(dev, &c);
    if (intf) *intf = f;
    if (cap) *cap = c;
    return (e1 == ESP_OK && e2 == ESP_OK) ? ESP_OK : (e1 != ESP_OK ? e1 : e2);
}
