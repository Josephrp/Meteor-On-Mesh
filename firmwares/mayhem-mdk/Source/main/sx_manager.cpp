/*
 * SXRadioManager implementation (Meshtonic H4M v2)
 *
 * Chip-selects are native ESP32-S3 GPIOs (shared SPI bus); per-radio BUSY and the
 * upper DIO1 lines are on an MCP23017. See sx_manager.hpp for the verified pin map.
 */
#include "sx_manager.hpp"
#include "lora_radio.h"
#include "meshtonic_board.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <algorithm>

static const char *TAG = "sx_mgr";

void SXRadioManager::init(int n) {
    setRadioCount(n);

    // Native chip-selects: configure as outputs, idle high (deasserted).
    uint64_t mask = 0;
    for (int s = 0; s < radioCount; s++) {
        int cs = NATIVE_CS[s];
        if (cs >= 0) mask |= (1ULL << cs);
    }
    if (mask) {
        gpio_config_t io = {};
        io.pin_bit_mask = mask;
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io);
        for (int s = 0; s < radioCount; s++) gpio_set_level((gpio_num_t)NATIVE_CS[s], 1);
    }
    csConfigured = true;

    // Shared LORA_RST (MCP GPB4) idle high; RF switch (GPB0) disabled until RX arms.
    if (g_mcp_ready) {
        meshtonic_mcp_write_pin((uint8_t)MCP_RST_SHARED, true);
        meshtonic_mcp_write_pin((uint8_t)MCP_RFSW, false);
    }

    activeSlot = (radioCount > 0 ? 0 : -1);
    ESP_LOGI(TAG, "SX manager init (v2): %d radios, native CS GPIO3/4/10/13, BUSY+DIO1(2-4) via MCP", radioCount);
}

void SXRadioManager::setRadioCount(int n) {
    radioCount = std::max(0, std::min(4, n));
}

bool SXRadioManager::selectSlot(int slot) {
    if (slot < 0 || slot >= radioCount || !csConfigured) return false;

    // Deassert all CS (idle high), then assert this slot's CS (active low).
    for (int s = 0; s < radioCount; s++) {
        gpio_set_level((gpio_num_t)NATIVE_CS[s], 1);
    }
    gpio_set_level((gpio_num_t)NATIVE_CS[slot], 0);
    activeSlot = slot;
    return true;
}

RadioSlotStatus SXRadioManager::getSlotStatus(int slot) {
    RadioSlotStatus st = {};
    if (slot < 0 || slot >= radioCount) return st;

    st.present = true; // assume attached if configured
    bool busy = false, dio = false;
    if (g_mcp_ready) mcp23017_read_pin(&g_mcp, (uint8_t)getBusyMcpPin(slot), &busy);
    if (isDio1Native(slot)) {
        dio = gpio_get_level((gpio_num_t)getDio1Gpio(slot)) != 0;
    } else if (g_mcp_ready) {
        mcp23017_read_pin(&g_mcp, (uint8_t)getDio1McpPin(slot), &dio);
    }

    st.busy = busy;
    st.dio1 = dio;
    st.rst_level = false; // shared net, not read back per-slot
    return st;
}

void SXRadioManager::spiSelectForSlot(int slot) {
    selectSlot(slot);
}

void SXRadioManager::spiDeselectAll() {
    if (!csConfigured) return;
    for (int s = 0; s < radioCount; s++) {
        gpio_set_level((gpio_num_t)NATIVE_CS[s], 1);
    }
}

int SXRadioManager::getCsGpio(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return NATIVE_CS[slot];
}
bool SXRadioManager::isDio1Native(int slot) const {
    return (slot >= 0 && slot < 4 && DIO1_NATIVE[slot] >= 0);
}
int SXRadioManager::getDio1Gpio(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return DIO1_NATIVE[slot];
}
int SXRadioManager::getDio1McpPin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_DIO1[slot];
}
int SXRadioManager::getBusyMcpPin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_BUSY[slot];
}

void SXRadioManager::serviceInterruptsFromIsr() {
    if (!g_mcp_ready) return;
    uint16_t intf = 0, cap = 0;
    // Best-effort I2C from ISR: only mark pending slots; SPI/IRQ drain runs in decoder task.
    if (mcp23017_read_interrupt_state(&g_mcp, &intf, &cap) != ESP_OK) return;

    for (int s = 0; s < radioCount && s < 4; s++) {
        if (isDio1Native(s)) continue; // native DIO1 (WIO1) is polled in the decoder task
        int d = getDio1McpPin(s);
        if (d != 0xFF && (intf & (1u << d))) {
            pendingIrqMask |= (uint8_t)(1u << s);
        }
    }
}

bool SXRadioManager::hasPendingIrq(int slot) const {
    if (slot < 0 || slot >= 4) return false;
    return (pendingIrqMask & (1u << slot)) != 0;
}

void SXRadioManager::clearPendingIrq(int slot) {
    if (slot < 0 || slot >= 4) return;
    pendingIrqMask &= (uint8_t)~(1u << slot);
}

uint8_t SXRadioManager::getPendingIrqMask() const {
    return pendingIrqMask;
}

void SXRadioManager::assignRadio(int slot, LoraRadio* radio) {
    if (slot >= 0 && slot < 4) radioInst[slot] = radio;
}

void SXRadioManager::servicePendingIrqs() {
    for (int s = 0; s < radioCount && s < 4; s++) {
        if (hasPendingIrq(s) && radioInst[s]) {
            radioInst[s]->handleDio1Irq();
            clearPendingIrq(s);
        }
    }
}
