/*
 * SXRadioManager implementation (Meshtonic)
 */
#include "sx_manager.hpp"
#include "lora_radio.h"
#include "meshtonic_board.h"
#include "esp_log.h"
#include <algorithm>

static const char *TAG = "sx_mgr";

void SXRadioManager::init(int n) {
    setRadioCount(n);
    // Default all CS deasserted (high). RST is shared; only write per-slot RST if a valid pin is assigned.
    if (g_mcp_ready) {
        for (int s = 0; s < radioCount; s++) {
            meshtonic_mcp_write_pin(getCsPin(s), true);   // CS idle high
            int rp = getRstPin(s);
            if (rp != 0xFF) meshtonic_mcp_write_pin(static_cast<unsigned char>(rp), true);
        }
    }
    activeSlot = (radioCount > 0 ? 0 : -1);
    ESP_LOGI(TAG, "SX manager init: %d radios (MCP pin map: WIO1 GPA3/5/7 etc)", radioCount);
}

void SXRadioManager::setRadioCount(int n) {
    radioCount = std::max(0, std::min(4, n));
}

bool SXRadioManager::selectSlot(int slot) {
    if (slot < 0 || slot >= radioCount || !g_mcp_ready) return false;

    // Deassert all CS
    for (int s = 0; s < radioCount; s++) {
        meshtonic_mcp_write_pin(getCsPin(s), true);
    }
    // Assert this slot's CS
    meshtonic_mcp_write_pin(getCsPin(slot), false);
    activeSlot = slot;
    return true;
}

RadioSlotStatus SXRadioManager::getSlotStatus(int slot) {
    RadioSlotStatus st = {};
    if (slot < 0 || slot >= radioCount || !g_mcp_ready) return st;

    st.present = true; // assume attached if configured
    bool busy=false, dio=false, rst=false;
    // Read inputs (best effort) using correct per-WIO MCP pins
    mcp23017_read_pin(&g_mcp, getBusyPin(slot), &busy);
    mcp23017_read_pin(&g_mcp, getDio1Pin(slot), &dio);
    int rp = getRstPin(slot);
    if (rp != 0xFF) mcp23017_read_pin(&g_mcp, static_cast<unsigned char>(rp), &rst);

    st.busy = busy;
    st.dio1 = dio;
    st.rst_level = rst;
    return st;
}

void SXRadioManager::spiSelectForSlot(int slot) {
    selectSlot(slot);
    // In a full impl: also drive the ESP SPI CS pin if using a shared GPIO CS, or rely on MCP CS only.
}

void SXRadioManager::spiDeselectAll() {
    if (!g_mcp_ready) return;
    for (int s = 0; s < radioCount; s++) {
        meshtonic_mcp_write_pin(getCsPin(s), true);
    }
}

int SXRadioManager::getCsPin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_CS[slot];
}
int SXRadioManager::getDio1Pin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_DIO1[slot];
}
int SXRadioManager::getBusyPin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_BUSY[slot];
}
int SXRadioManager::getRstPin(int slot) const {
    if (slot < 0 || slot >= 4) return 0xFF;
    return MCP_RST[slot];
}

void SXRadioManager::serviceInterruptsFromIsr() {
    if (!g_mcp_ready) return;
    uint16_t intf = 0, cap = 0;
    // Best-effort I2C from ISR: only mark pending slots; SPI/IRQ drain runs in decoder task.
    if (mcp23017_read_interrupt_state(&g_mcp, &intf, &cap) != ESP_OK) return;

    for (int s = 0; s < radioCount && s < 4; s++) {
        int d = getDio1Pin(s);
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
