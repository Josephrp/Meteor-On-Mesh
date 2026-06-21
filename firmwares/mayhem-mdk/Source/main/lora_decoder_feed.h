#pragma once

#include <cstdint>
#include "ppi2c/pp_structures.hpp"  // brings in lora_packet_compact_t, lora_rich_status_t for rich UI accessors

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

// Rich UI ("apps over I2C") accessors — used by custom command handlers for the native PP screen app.
// These return data from the active decoder (WIO or hybrid). Implemented in ep_app_loradecoder.cpp.
size_t lora_get_recent_compact(lora_packet_compact_t* out, size_t max_count);
void   lora_get_rich_status(lora_rich_status_t* out);

// Apply preset coming from the rich PP screen UI (apps over I2C). Returns true on success.
bool lora_rich_apply_preset(const char* preset_id);

// Start/stop decoder session from rich PP UI (0xa024 control op: 0=stop, 1=start).
bool lora_rich_control(uint8_t op);
