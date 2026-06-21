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

enum LoraProtoId : uint8_t {
    LORA_PROTO_UNKNOWN = 0,
    LORA_PROTO_MESHTASTIC = 1,
    LORA_PROTO_MESHCORE = 2,
    LORA_PROTO_LORAWAN = 3,
    LORA_PROTO_LORAMESHER = 4,
    LORA_PROTO_LORA_APRS = 5,
    LORA_PROTO_RETICULUM = 6,
    LORA_PROTO_DISASTER_RADIO = 7,
    LORA_PROTO_RADIOHEAD = 8,
    LORA_PROTO_EBYTE_LORA = 9,
};

enum LoraDecodeMode : uint8_t {
    LORA_DECODE_CPP = 0,
    LORA_DECODE_PYTHON = 1,
    LORA_DECODE_AUTO = 2,
};

// On-device backends (no host PC required).
enum LoraBackend : uint8_t {
    LORA_BACKEND_HOST_BRIDGE = 0,  // optional WiFi ingest from host
    LORA_BACKEND_WIO = 1,          // 4x SX1262 narrowband (standalone H4M)
    LORA_BACKEND_HACKRF_DSP = 2,   // HackRF IQ bursts via PortaPack I2C
    LORA_BACKEND_HYBRID = 3,       // WIO + HackRF bursts simultaneously
};

struct LoraPacket {
    uint32_t ts_ms = 0;
    float    freq_mhz = 0;
    uint32_t bw_hz = 0;
    uint8_t  sf = 0;
    int16_t  rssi = 0;
    int8_t   snr = 0;
    std::string payload_hex;
    uint8_t  proto = 0;
    std::string info;
    uint8_t  slot = 0;
    char region[16] = {};
    char profile[24] = {};
    char preset_id[32] = {};
    char band[16] = {};
    char confidence[12] = "candidate";
    bool decrypted = false;
    char key_label[24] = {};
    char decode_backend[8] = "cpp";
};

enum class RadioRxMode : uint8_t {
    CAD = 0,
    CONTINUOUS = 1,
    AUTO = 2,
};

struct RadioChannel {
    float freq_mhz = 906.875f;
    uint8_t sf = 11;
    uint32_t bw_hz = 250000;
    uint8_t cr = 5;
    RadioRxMode rx_mode = RadioRxMode::CAD;
    bool cad_after_rx = true;
    char region[16] = "US915";
    char profile[24] = "meshtastic_longfast";
    char preset_id[32] = "US915-meshtastic";
};

struct LoraDecoderConfig {
    float center_mhz = 915.0f;
    uint32_t bw_mask = 0xFFFF;
    uint8_t sf_mask  = 0xFF;
    uint8_t radio_count = 1;
    uint8_t backend = 1;
    char keys[128] = {0};
    RadioChannel channels[4];
    RadioRxMode default_rx_mode = RadioRxMode::CAD;
    bool global_cad_after_rx = true;
    uint8_t slot_present_mask = 0;
    char active_preset_id[32] = {};
    LoraDecodeMode decode_mode = LORA_DECODE_CPP;
    char sidecar_url[96] = {};
};

struct BandUsageAgg {
    char region[16] = {};
    char profile[24] = {};
    uint8_t slots_mask = 0;
    uint32_t pkts = 0;
    uint32_t last_ts = 0;
    uint32_t proto_counts[10] = {};
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

    bool applyPresetId(const char* preset_id);
    bool applyPresetToSlot(int slot, const char* preset_id);

    bool isRunning() const { return running; }

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
    void tagPacketFromSlot(LoraPacket& p, int slot);
    void updateBandAgg(const LoraPacket& p);
    void resetBandAgg();
    void handleFeedIq(const std::vector<uint8_t>& data);
    void processIqBuffer();
    void maybeUpgradeHybridBackend();
    static const char* backendName(uint8_t backend);
    static RadioRxMode parseRxModeToken(const char* tok);
    static const char* rxModeName(RadioRxMode m);
    static const char* protoName(uint8_t proto);

    bool running = false;
    bool rearming = false;
    uint32_t arm_generation = 0;
    uint32_t lastPushMs = 0;
    LoraDecoderConfig config;

    static constexpr size_t MAX_PACKETS = 32;
    std::vector<LoraPacket> packets;

    static constexpr int MAX_RADIOS = 4;
    static constexpr int MAX_BAND_AGG = 8;
    LoraRadio* radios[MAX_RADIOS] = {nullptr};
    LoraConfig radioCfgs[MAX_RADIOS];
    RadioRxMode active_rx_mode[MAX_RADIOS] = {};
    int8_t last_rssi[MAX_RADIOS] = {};
    uint32_t last_rx_ms[MAX_RADIOS] = {};
    char last_key_label[MAX_RADIOS][24] = {};
    BandUsageAgg band_agg[MAX_BAND_AGG];
    int num_band_agg = 0;
    int numActiveRadios = 0;

    static constexpr size_t IQ_MAX_SAMPLES = 4096;
    bool iq_active = false;
    float iq_center_mhz = 915.0f;
    float iq_fs = 2000000.0f;
    uint32_t iq_expected = 0;
    uint32_t iq_received = 0;
    int16_t iq_buf[IQ_MAX_SAMPLES * 2] = {};
    uint32_t hackrf_burst_count = 0;
    uint32_t total_packet_count = 0;
};

#endif  // EP_APP_LORADECODER_HPP
