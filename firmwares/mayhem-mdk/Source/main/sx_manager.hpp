/*
 * Configurable 1-4 Wio SX1262 radio slot manager for Meshtonic H4M v2.
 *
 * Shared SPI bus + per-slot NATIVE chip-select (GPIO matrix). Per-radio BUSY and
 * the upper DIO1 lines are read through an MCP23017 I2C expander; WIO1 DIO1 is a
 * native GPIO. Pin map verified against pcb/meshtonic_h4m_v2.kicad_pcb (see
 * pcb/refs/v1 for the obsolete single-radio v1 map this replaces).
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

    bool selectSlot(int slot);                 // drives native CS for slot, deasserts others
    RadioSlotStatus getSlotStatus(int slot);

    // Low-level hook points (SPI + expander) for the SX126x driver.
    void spiSelectForSlot(int slot);
    void spiDeselectAll();

    // --- Meshtonic H4M v2 pin-map accessors ---
    // Chip-selects are native ESP32-S3 GPIOs (shared SPI bus). Returns 0xFF if invalid.
    int getCsGpio(int slot) const;
    // DIO1: slot 0 (WIO1) is a native GPIO; slots 1-3 are on the MCP23017 GPA bank.
    bool isDio1Native(int slot) const;
    int getDio1Gpio(int slot) const;     // native ESP32 GPIO (valid when isDio1Native)
    int getDio1McpPin(int slot) const;   // MCP pin 0-15 (valid when !isDio1Native), else 0xFF
    // BUSY: all on the MCP23017 GPA bank.
    int getBusyMcpPin(int slot) const;   // MCP pin 0-15, else 0xFF
    // Shared control lines on the MCP23017 GPB bank.
    int getSharedRstMcpPin() const { return MCP_RST_SHARED; } // GPB4 (one net for all radios)
    int getRfSwEnMcpPin() const { return MCP_RFSW; }          // GPB0
    // MCP INTA is wired to a native ESP32-S3 GPIO.
    int getMcpIntaGpio() const { return MCP_INTA_GPIO; }      // GPIO44 (D7)

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
    bool csConfigured = false;
    volatile uint8_t pendingIrqMask = 0; // bit per slot for DIO1 events from MCP INTA fan-in
    LoraRadio* radioInst[4] = {nullptr, nullptr, nullptr, nullptr};

    /*
     * Meshtonic H4M v2 map (real ESP32-S3 silicon GPIOs; XIAO ESP32-S3 Plus Dn!=GPIOn):
     *   CS  (native): WIO1=GPIO3*, WIO2=GPIO4, WIO3=GPIO10, WIO4=GPIO13   (*strapping pin)
     *   BUSY (MCP):   GPA0..GPA3
     *   DIO1:         WIO1 native GPIO12; WIO2..4 on MCP GPA4..GPA6
     *   Shared:       LORA_RST = MCP GPB4, RF switch enable = MCP GPB0
     *   MCP INTA  ->  native GPIO44 (D7)
     * Shared SPI bus (set at LoraRadio::init): MOSI=GPIO38, MISO=GPIO39, SCK=GPIO40.
     */
    static constexpr int NATIVE_CS[4]   = {3, 4, 10, 13};
    static constexpr int DIO1_NATIVE[4] = {12, -1, -1, -1};      // -1 => DIO1 is on the MCP
    static constexpr int MCP_DIO1[4]    = {0xFF, 4, 5, 6};       // GPA4/5/6 for slots 1-3
    static constexpr int MCP_BUSY[4]    = {0, 1, 2, 3};          // GPA0..GPA3
    static constexpr int MCP_RST_SHARED = 12;                    // GPB4 (shared LORA_RST)
    static constexpr int MCP_RFSW       = 8;                     // GPB0 (WIO_RF_SW_EN)
    static constexpr int MCP_INTA_GPIO  = 44;                    // native D7/GPIO44
};
