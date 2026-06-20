#pragma once
/*
 * SX1262 LoRa radio driver for Meshtonic H4M (ESP-IDF, Mayhem MDK).
 *
 * Uses the existing SXRadioManager (MCP23017 slot selection) + shared SPI.
 * The "hardware antennae" on the board (1-4 Wio SX1262 modules) are the receivers.
 *
 * This enables the vendored "heavy DSP" / decoder tasks (detection via CAD,
 * receive, metrics, software post-processing of packets) directly on the ESP32
 * using the board's own LoRa hardware instead of an external HackRF.
 */

#include <stdint.h>
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Radio configuration for one "virtual channel" / monitor task.
struct LoraConfig {
    uint32_t freq_hz;     // e.g. 915000000
    uint8_t  sf;          // 7..12
    uint32_t bw_hz;       // 125000, 250000, 500000
    uint8_t  cr;          // 4/5 .. 4/8 -> 1..4
    uint8_t  ldro;        // low data rate optimize
    uint16_t preamble_syms;
    bool     explicit_header;
    bool     crc_on;
};

class LoraRadio {
public:
    LoraRadio(int slot);

    esp_err_t init(spi_host_device_t spi_host, int mosi, int miso, int sclk, int freq_hz = 10000000);
    void deinit();

    // Select this radio on the MCP expander (CS active)
    bool select();

    // Basic bring-up (full sequence recommended)
    esp_err_t reset();
    esp_err_t setStandby();
    esp_err_t setPacketTypeLoRa();
    esp_err_t setFrequency(uint32_t freq_hz);
    esp_err_t setModulationParams(uint8_t sf, uint32_t bw_hz, uint8_t cr, uint8_t ldro);
    esp_err_t setPacketParams(uint16_t preamble, bool explicitHeader, uint8_t payloadLen, bool crcOn, bool invertIQ = false);
    esp_err_t setSyncWord(uint16_t syncWord); // 0x1424 for Meshtastic/private LoRa
    esp_err_t setDio2AsRfSwitch(bool enable);
    esp_err_t setDio3TcxoCtrl(float voltageVolts); // e.g. 1.8 per variant
    esp_err_t setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask);

    // Reliability / calibration helpers (see SX126x DS 13.4 / RadioLib SX126x.cpp)
    esp_err_t setRxBoost(bool enable);     // higher LNA gain for CAD/RX sensitivity
    esp_err_t setCurrentLimit(uint8_t ma); // OCP trim, typical 60-140mA
    esp_err_t getDeviceErrors(uint16_t* err); // status reporting
    esp_err_t setRx(uint32_t timeout_symbols = 0);   // 0 = continuous
    esp_err_t setCad();
    esp_err_t setTx(uint32_t timeout_symbols = 0);

    // Status / events (poll or after IRQ)
    uint8_t  getStatus();
    bool     isBusy();
    bool     readDio1();   // via MCP
    bool     readBusy();

    esp_err_t getIrqStatus(uint16_t* irq);
    esp_err_t clearIrqStatus(uint16_t mask);
    esp_err_t getRxBufferStatus(uint8_t* payloadLen, uint8_t* rxStartBufferPointer);

    // Packet handling (accurate)
    esp_err_t getPacketStatus(int8_t* rssiPkt, int8_t* snrPkt, int8_t* signalRssiPkt = nullptr);
    esp_err_t readBuffer(uint8_t* buf, uint8_t* len, uint8_t maxLen);

    // High level helpers
    esp_err_t configureFor(const LoraConfig& cfg);
    esp_err_t startCad();
    esp_err_t startRxContinuous();

    // Monitor-only guard: blocks setTx when LoRa decoder session is active
    static void setMonitorMode(bool enabled);

    // Returns true if a packet is ready (call after DIO1 or poll)
    bool     checkRxDone(uint8_t* outLen, uint8_t maxLen, uint8_t* outBuf, int8_t* outRssi, int8_t* outSnr);

    // Called when MCP INTA + this slot's DIO1 was the source (from sxManager pending)
    void handleDio1Irq();

    int getSlot() const { return slot; }

private:
    int slot;
    spi_device_handle_t spi_dev = nullptr;
    bool selected = false;

    esp_err_t writeCommand(uint8_t cmd, const uint8_t* data, size_t len);
    esp_err_t readCommand(uint8_t cmd, uint8_t* data, size_t len);
    esp_err_t writeRegister(uint16_t addr, uint8_t val);
    esp_err_t readRegister(uint16_t addr, uint8_t* val);
    esp_err_t writeBuffer(const uint8_t* data, uint8_t len);
};

#ifdef __cplusplus
}
#endif

// C-callable alias for ep_app_loradecoder
inline void lora_radio_set_monitor_mode(bool enabled) { LoraRadio::setMonitorMode(enabled); }
