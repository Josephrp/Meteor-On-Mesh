/*
 * Meshtonic H4M board support (thin wrappers)
 */
#include "meshtonic_board.h"
#include "esp_log.h"

static const char *TAG = "meshtonic";

tca9548a_t g_tca;
bool g_tca_ready = false;
mcp23017_t g_mcp;
bool g_mcp_ready = false;

esp_err_t meshtonic_tca_select(uint8_t ch) {
    if (!g_tca_ready) return ESP_OK; // no-op if not present
    if (ch > 7) return ESP_ERR_INVALID_ARG;
    return tca9548a_select_channel(&g_tca, ch);
}

esp_err_t meshtonic_mcp_write_pin(uint8_t pin, bool level) {
    if (!g_mcp_ready) return ESP_OK;
    if (pin > 15) return ESP_ERR_INVALID_ARG;
    return mcp23017_write_pin(&g_mcp, pin, level);
}

#include "sensordb.h"
esp_err_t meshtonic_select_sensor_channel(uint8_t addr) {
    if (!g_tca_ready) return ESP_OK; // direct bus or not H4M
    uint8_t ch = getSensorChannel(addr);
    if (ch <= 7) {
        ESP_LOGD(TAG, "select ch%u for addr 0x%02x", ch, addr);
        return tca9548a_select_channel(&g_tca, ch);
    }
    // 0xFE or unknown: leave deselected or on 0; caller will talk direct or last-selected
    return ESP_OK;
}
