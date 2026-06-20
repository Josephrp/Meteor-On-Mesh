#pragma once

#include "driver/spi_master.h"
#include "esp_err.h"

/**
 * Lightweight shared SPI bus manager for Meshtonic H4M.
 * Ensures the bus (typically SPI2_HOST, pins 11/12/13) is initialized exactly once
 * with DMA, so radios + future display (ILI9341) can share without re-init races.
 *
 * Usage: call spi_bus_manager_init(...) before adding any devices.
 * Safe to call multiple times; subsequent calls are no-ops or return ESP_OK if already up.
 */

esp_err_t spi_bus_manager_init(spi_host_device_t host, int mosi, int miso, int sclk, int max_transfer_sz = 4096);

bool spi_bus_manager_is_initialized(spi_host_device_t host);