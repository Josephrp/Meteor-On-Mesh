/*
 * Meshtonic H4M board support (ESP-IDF)
 * Provides access to TCA9548A and MCP23017 when running on the Meshtonic profile.
 */
#pragma once

#include <stdbool.h>
#include "drivers/tca9548a.h"
#include "drivers/mcp23017.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tca9548a_t g_tca;
extern bool g_tca_ready;
extern mcp23017_t g_mcp;
extern bool g_mcp_ready;

/** Select a TCA channel before talking to a device behind it. Safe no-op if not ready. */
esp_err_t meshtonic_tca_select(uint8_t ch);

/** Write a single MCP pin (for radio CS etc). Safe no-op if not ready. */
esp_err_t meshtonic_mcp_write_pin(uint8_t pin, bool level);

/** For H4M sensors behind TCA: select the recorded channel for a given I2C addr (no-op if not muxed/unknown). */
esp_err_t meshtonic_select_sensor_channel(uint8_t addr);

#ifdef __cplusplus
}
#endif
