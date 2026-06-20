#ifndef EP_APP_LORADECODER_HPP
#define EP_APP_LORADECODER_HPP

/*
 * LoRa Wideband Decoder application for ESP32PP / Mayhem MDK.
 */

#include "ep_app.hpp"
#include "pp_commands.hpp"
#include "../lora_dsp/lora_dsp.h"
#include "../lora_radio.h"
#include <vector>
#include <string>
#include <cstdint>

struct LoraPacket {
    uint32_t ts_ms;
    float    freq_mhz;
    uint32_t bw_hz;
    uint8_t  sf;
    int16_t  rssi;
    int8_t   snr;
    std::string payload_hex;
    uint8_t  proto;
    std::string info;
};

enum class RadioRxMode : uint8_t {
    CAD = 0,
    CONTINUOUS = 1,
    AUTO = 2,
};

struct RadioChannel {
    float freq_mhz = 915.0f;
    uint8_t sf = 12;
    uint32_t bw_hz = 125000;
    uint8_t cr = 1;
    RadioRxMode rx_mode = RadioRxMode::CAD;
    bool cad_after_rx = true;
};

struct LoraDecoderConfig {
    float center_mhz = 915.0f;
    uint32_t bw_mask = 0xFFFF;
    uint8_t sf_mask  = 0xFF;
    uint8_t radio_count = 1;
    uint8_t backend = 0;
    char keys[128] = {0}; // legacy single-key string fallback
    RadioChannel channels[4];
    RadioRxMode default_rx_mode = RadioRxMode::CAD;
    bool global_cad_after_rx = true;
    uint8_t slot_present_mask = 0; // runtime: bit i set if slot i responded at arm
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

    void pushPacket(const LoraPacket& pkt);
    void feedIQ_sc16(const int16_t* iq, size_t count, float fs, float center_mhz);

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
    bool loadConfigFromNvs();
    void persistConfigToNvs();
    RadioRxMode resolveRxMode(int slot) const;
    void startRxForSlot(int slot);
    void applyRxPolicyAfterPacket(int slot, uint32_t now_ms);
    static RadioRxMode parseRxModeToken(const char* tok);
    static const char* rxModeName(RadioRxMode m);

    bool running = false;
    bool rearming = false;
    uint32_t arm_generation = 0;
    uint32_t lastPushMs = 0;
    LoraDecoderConfig config;

    static constexpr size_t MAX_PACKETS = 32;
    std::vector<LoraPacket> packets;

    static constexpr int MAX_RADIOS = 4;
    LoraRadio* radios[MAX_RADIOS] = {nullptr};
    LoraConfig radioCfgs[MAX_RADIOS];
    RadioRxMode active_rx_mode[MAX_RADIOS] = {};
    int8_t last_rssi[MAX_RADIOS] = {};
    uint32_t last_rx_ms[MAX_RADIOS] = {};
    char last_key_label[MAX_RADIOS][24] = {};
    int numActiveRadios = 0;
};

#endif  // EP_APP_LORADECODER_HPP
