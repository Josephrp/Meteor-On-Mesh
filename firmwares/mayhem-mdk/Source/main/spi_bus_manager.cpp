#include "spi_bus_manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

static const char *TAG = "spi_bus_mgr";

static bool s_initialized[SPI_HOST_MAX] = {false};

esp_err_t spi_bus_manager_init(spi_host_device_t host, int mosi, int miso, int sclk, int max_transfer_sz) {
    if (host < 0 || host >= SPI_HOST_MAX) return ESP_ERR_INVALID_ARG;
    if (s_initialized[host]) {
        return ESP_OK; // already up
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = mosi;
    buscfg.miso_io_num = miso;
    buscfg.sclk_io_num = sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = max_transfer_sz;

    esp_err_t err = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    if (err == ESP_OK) {
        s_initialized[host] = true;
        ESP_LOGI(TAG, "SPI bus %d initialized (mosi=%d miso=%d sclk=%d dma)", (int)host, mosi, miso, sclk);
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        // Already initialized by someone else (display etc). Treat as success.
        s_initialized[host] = true;
        ESP_LOGI(TAG, "SPI bus %d was already initialized (reusing)", (int)host);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "spi_bus_initialize(%d) failed: %s", (int)host, esp_err_to_name(err));
    return err;
}

bool spi_bus_manager_is_initialized(spi_host_device_t host) {
    if (host < 0 || host >= SPI_HOST_MAX) return false;
    return s_initialized[host];
}