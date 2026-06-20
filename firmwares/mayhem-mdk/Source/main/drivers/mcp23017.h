/*
 * MCP23017 16-bit I2C GPIO expander driver (ESP-IDF)
 * Used on Meshtonic H4M to control 1-4 Wio SX1262 radios (CS, DIO1, BUSY, RESET, etc.)
 */

#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "i2cdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCP23017_ADDR_BASE 0x20

// Register addresses (IOCON.BANK=0)
#define MCP_REG_IODIRA   0x00
#define MCP_REG_IODIRB   0x01
#define MCP_REG_IPOLA    0x02
#define MCP_REG_IPOLB    0x03
#define MCP_REG_GPINTENA 0x04
#define MCP_REG_GPINTENB 0x05
#define MCP_REG_DEFVALA  0x06
#define MCP_REG_DEFVALB  0x07
#define MCP_REG_INTCONA  0x08
#define MCP_REG_INTCONB  0x09
#define MCP_REG_IOCON    0x0A
#define MCP_REG_GPPUA    0x0C
#define MCP_REG_GPPUB    0x0D
#define MCP_REG_INTFA    0x0E
#define MCP_REG_INTFB    0x0F
#define MCP_REG_INTCAPA  0x10
#define MCP_REG_INTCAPB  0x11
#define MCP_REG_GPIOA    0x12
#define MCP_REG_GPIOB    0x13
#define MCP_REG_OLATA    0x14
#define MCP_REG_OLATB    0x15

typedef struct {
    i2c_dev_t dev;
    uint16_t dir;   // cached direction (1=input)
    uint16_t out;   // cached output latch
} mcp23017_t;

/** Init descriptor + basic setup (seqop off, all outputs default low after reset) */
esp_err_t mcp23017_init_desc(mcp23017_t *dev, uint8_t addr, i2c_port_t port,
                             gpio_num_t sda_gpio, gpio_num_t scl_gpio);

esp_err_t mcp23017_free_desc(mcp23017_t *dev);

/** Write full 16-bit direction (1 = input). A0..A7 = bits 0..7, B0..B7 = bits 8..15 */
esp_err_t mcp23017_set_direction(mcp23017_t *dev, uint16_t dir_mask);

/** Set a single pin direction: pin 0-15, true=input */
esp_err_t mcp23017_set_pin_direction(mcp23017_t *dev, uint8_t pin, bool input);

/** Write full 16-bit output latch */
esp_err_t mcp23017_write_port(mcp23017_t *dev, uint16_t value);

/** Write single pin (0-15) */
esp_err_t mcp23017_write_pin(mcp23017_t *dev, uint8_t pin, bool level);

/** Read full GPIO */
esp_err_t mcp23017_read_port(mcp23017_t *dev, uint16_t *value);

/** Read single pin */
esp_err_t mcp23017_read_pin(mcp23017_t *dev, uint8_t pin, bool *level);

/** Enable/disable pull-up on pin */
esp_err_t mcp23017_set_pullup(mcp23017_t *dev, uint8_t pin, bool enable);

/** Configure interrupt on change for a pin (INTPOL etc via IOCON separately if needed) */
esp_err_t mcp23017_set_interrupt(mcp23017_t *dev, uint8_t pin, bool enable);

/** Read INTF (which pins caused interrupt) */
esp_err_t mcp23017_get_intf(mcp23017_t *dev, uint16_t *intf);

/** Read INTCAP (GPIO state captured at time of interrupt) */
esp_err_t mcp23017_get_intcap(mcp23017_t *dev, uint16_t *cap);

/** Read both INTF+INTCAP atomically (useful for dispatch) */
esp_err_t mcp23017_read_interrupt_state(mcp23017_t *dev, uint16_t *intf, uint16_t *cap);

#ifdef __cplusplus
}
#endif
