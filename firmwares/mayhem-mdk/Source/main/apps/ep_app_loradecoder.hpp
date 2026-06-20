#ifndef EP_APP_LORADECODER_HPP
#define EP_APP_LORADECODER_HPP

/*
 * LoRa Wideband Decoder application for ESP32PP / Mayhem MDK.
 *
 * This app provides the on-device surface for the vendored LoRa Wideband Decoder.
 *
 * Vendored source lives at: ../lora-wideband-decoder/ (relative to Source) or
 * firmwares/mayhem-mdk/lora-wideband-decoder/ from the project root.
 *
 * Responsibilities on the ESP32 (as implemented in Mayhem MDK):
 * - Own configuration for wideband / multi-SF monitoring sessions.
 * - Accept decoded packet events from a host running the vendored Python decoder
 *   (via HTTP POST bridge or future websocket/uart).
 * - Provide local narrowband monitoring using the board's 1-4 WIO SX1262 radios
 *   (MCP23017 routed). Full wideband IQ capture requires external HackRF + host.
 * - Expose recent packets + status to:
 *     * Web UI / websocket (browser on the ESP AP or STA)
 *     * PortaPack over I2C (custom PP commands)
 *     * Display (if present)
 * - Bridge control: start/stop external decoder session hints, arm local radios.
 */

#include "ep_app.hpp"
#include "pp_commands.hpp"
#include "../lora_dsp/lora_dsp.h"
#include "../lora_radio.h"
#include <vector>
#include <string>

struct LoraPacket {
    uint32_t ts_ms;          // device uptime ms when received
    float    freq_mhz;
    uint32_t bw_hz;
    uint8_t  sf;             // 7-12
    int16_t  rssi;
    int8_t   snr;            // 0.25 dB units or raw
    std::string payload_hex; // up to ~256 bytes worth, truncated if needed
    uint8_t  proto;          // 0=unknown, 1=meshtastic, 2=lorawan, ...
    std::string info;        // short decoded summary (node ids, etc.)
};

struct RadioChannel {
    float freq_mhz = 915.0f;
    uint8_t sf = 12;
    uint32_t bw_hz = 125000;
    uint8_t cr = 1; // 4/5
};

struct LoraDecoderConfig {
    float center_mhz = 915.0f;
    uint32_t bw_mask = 0xFFFF;   // bitmask of enabled BWs (placeholder)
    uint8_t sf_mask  = 0xFF;     // bits 7-12
    uint8_t radio_count = 1;     // how many local slots to use
    uint8_t backend = 0;         // 0=host bridge (Python LWD+HackRF), 1=local SX radios (MCP), 2=embedded DSP (vendored C++ on ESP32 from HackRF IQ)
    char keys[128] = {0};        // base64 or hex keys for Meshtastic etc. (simplified)
    RadioChannel channels[4];    // per-slot config; populated from center or NVS/web
};

class EPAppLoraDecoder : public EPApp {
public:
    EPAppLoraDecoder();
    ~EPAppLoraDecoder() override = default;

    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;

    bool OnWebData(std::string& data) override;

    void OnDisplayRequest(DisplayGeneric* display) override;
    void Loop(uint32_t currentMillis) override;

    // Called by webserver POST bridge (or other feeders) to inject a decoded packet (host path)
    void pushPacket(const LoraPacket& pkt);

    // Feed raw IQ (sc16) directly into the embedded DSP backend (2).
    // This is the path for "ESP32 processes HackRF IQ locally".
    void feedIQ_sc16(const int16_t* iq, size_t count, float fs, float center_mhz);

    // Control
    void startSession();
    void stopSession();
    void setConfig(const LoraDecoderConfig& cfg);
    LoraDecoderConfig getConfig() const { return config; }

    std::vector<LoraPacket> getRecentPackets(size_t maxCount = 16) const;

private:
    void sendStatusToWeb();
    void sendPacketsToWeb(size_t count = 8);
    void armLocalRadios();
    void disarmLocalRadios();

    bool running = false;
    uint32_t lastPushMs = 0;
    LoraDecoderConfig config;

    static constexpr size_t MAX_PACKETS = 32;
    std::vector<LoraPacket> packets;  // ring-like, newest at back

    // Onboard radio support (uses Meshtonic's own SX1262 + antennas, not HackRF)
    static constexpr int MAX_RADIOS = 4;
    LoraRadio* radios[MAX_RADIOS] = {nullptr};
    LoraConfig radioCfgs[MAX_RADIOS];
    int numActiveRadios = 0;
};

#endif  // EP_APP_LORADECODER_HPP
