/*
 * Configurable 1-4 Wio SX1262 radio slot manager for Meshtonic H4M
 * Uses MCP23017 for per-radio CS / DIO1 / BUSY / RST
 */
#pragma once

#include <cstdint>

class LoraRadio; // forward for IRQ wiring

struct RadioSlotStatus {
    bool present;   // heuristic: was ever selected / responded
    bool busy;
    bool dio1;
    bool rst_level;
};

class SXRadioManager {
public:
    void init(int radioCount);
    void setRadioCount(int n);

    int getRadioCount() const { return radioCount; }
    int getActiveSlot() const { return activeSlot; }

    bool selectSlot(int slot);                 // sets CS for slot, deasserts others
    RadioSlotStatus getSlotStatus(int slot);

    // Low-level hook points (SPI + expander) for a real SX126x driver later
    void spiSelectForSlot(int slot);
    void spiDeselectAll();

    // Pin map accessors (public so main/init can set directions / IRQs using correct WIO pins)
    int getCsPin(int slot) const;
    int getDio1Pin(int slot) const;
    int getBusyPin(int slot) const;
    int getRstPin(int slot) const; // returns 0xFF if not per-radio on MCP

    // Interrupt fan-in support (MCP INTA -> dispatch to slot)
    void serviceInterruptsFromIsr(); // safe to call from ISR or task: reads INTF/INTCAP, sets pending
    bool hasPendingIrq(int slot) const;
    void clearPendingIrq(int slot);
    uint8_t getPendingIrqMask() const; // bit i set if slot i has DIO1 event pending

    // Wire LoraRadio instances for direct dispatch from IRQ service (4 slots)
    void assignRadio(int slot, LoraRadio* radio);
    void servicePendingIrqs(); // calls handleDio1Irq on assigned radios that have pending bits

private:
    int radioCount = 0;
    int activeSlot = -1;
    volatile uint8_t pendingIrqMask = 0; // bit per slot for DIO1 events from MCP INTA fan-in
    LoraRadio* radioInst[4] = {nullptr, nullptr, nullptr, nullptr};

    // Correct MCP23017 pin map for Meshtonic H4M WIO modules (from PCB netlist + Meshtonic variant.h / PIN_AUDIT.md)
    // WIO1 (frozen): CS=GPA3, DIO1=GPA5, BUSY=GPA7
    // WIO2: CS=GPA4, DIO1=GPA6, BUSY=GPB0
    // WIO3: CS=GPA0, DIO1=GPA1, BUSY=GPA2
    // WIO4: CS/DIO1/BUSY on GPB (assignment requires final hardware audit vs full v2 netlist)
    // RST is a shared LORA_RST net (no per-radio MCP drive in reference); getRstPin returns 0xFF to indicate skip.
    static constexpr int MCP_CS[4]   = {3, 4, 0, 9};   // GPA3, GPA4, GPA0, GPB1
    static constexpr int MCP_DIO1[4] = {5, 6, 1, 10};  // GPA5, GPA6, GPA1, GPB2
    static constexpr int MCP_BUSY[4] = {7, 8, 2, 11};  // GPA7, GPB0, GPA2, GPB3
    static constexpr int MCP_RST[4]  = {0xFF, 0xFF, 0xFF, 0xFF}; // shared or NC; 0xFF = do not drive per-slot
};
