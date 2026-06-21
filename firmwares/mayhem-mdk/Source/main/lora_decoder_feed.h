#pragma once

#include <cstdint>

struct LoraDecodedRecord {
    uint32_t ts_ms;
    float freq_mhz;
    uint32_t bw_hz;
    uint8_t sf;
    int16_t rssi;
    int8_t snr;
    uint8_t slot;
    uint8_t proto;
    bool decrypted;
    char region[16];
    char profile[24];
    char preset_id[32];
    char band[16];
    char confidence[12];
    char key_label[24];
    char decode_backend[8];
    char payload_hex[513];
    char info[128];
};

void lora_decoder_push_record(const LoraDecodedRecord& rec);

void lora_decoder_push_packet(uint32_t ts_ms,
                              float freq_mhz,
                              uint32_t bw_hz,
                              uint8_t sf,
                              int16_t rssi,
                              int8_t snr,
                              const char* payload_hex,
                              uint8_t proto,
                              const char* info);
