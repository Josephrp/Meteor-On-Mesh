#pragma once

/*
 * On-device Meshtastic decode (AES-CTR + minimal protobuf + validation gates).
 * Port of core logic from vendored LWD decoder.py.
 */

#include <cstdint>
#include <cstddef>
#include <string>

struct LoraPacket; // from ep_app_loradecoder.hpp

struct LoraDecodeOutcome {
    bool header_ok = false;
    bool decrypted = false;
    bool valid = false;       // passed KNOWN_PORTNUMS + UTF-8 gates
    bool encrypted_only = false; // header ok but no matching key
    uint32_t to = 0;
    uint32_t from = 0;
    uint32_t pkt_id = 0;
    uint8_t flags = 0;
    uint8_t channel_hash = 0;
    int portnum = -1;
    char key_label[24] = {};
    char node_from[12] = {};  // Meshtastic !id display form
    char node_to[12] = {};
};

// --- Key list (structured; NVS + web) ---------------------------------------
constexpr int LORA_DECODE_MAX_KEYS = 8;
constexpr int LORA_DECODE_MAX_NODES_PER_KEY = 8;

enum LoraDecodeKeyScope : uint8_t {
    LORA_KEY_SCOPE_ALL = 0,
    LORA_KEY_SCOPE_NODES = 1,
};

struct LoraDecodeKeyEntry {
    char label[20] = {};
    uint8_t key[32] = {};
    uint8_t key_len = 0;
    LoraDecodeKeyScope scope = LORA_KEY_SCOPE_ALL;
    uint8_t priority = 50;
    bool enabled = true;
    char node_ids[LORA_DECODE_MAX_NODES_PER_KEY][12] = {}; // Meshtastic !id form
    uint8_t node_count = 0;
};

struct LoraDecodeKeyConfig {
    bool try_default = true;
    uint8_t default_priority = 50;
    uint8_t entry_count = 0;
    LoraDecodeKeyEntry entries[LORA_DECODE_MAX_KEYS] = {};
};

void lora_decode_keys_init_defaults();
const LoraDecodeKeyConfig& lora_decode_keys_get();
void lora_decode_keys_set(const LoraDecodeKeyConfig& cfg);
bool lora_decode_keys_load_nvs();
bool lora_decode_keys_save_nvs();
void lora_decode_keys_set_legacy_string(const char* keys); // legacy flat string fallback
bool lora_decode_keys_parse_web(const char* keys_body);    // structured web KEYS body

// --- Crypto / protobuf helpers (testable) -----------------------------------
void lora_build_meshtastic_nonce(uint32_t pkt_id, uint32_t sender, uint8_t nonce[16]);
bool lora_aes_ctr_decrypt(const uint8_t* key, size_t key_len,
                          const uint8_t nonce[16],
                          const uint8_t* in, size_t in_len, uint8_t* out);

bool lora_is_known_portnum(int portnum);
bool lora_is_text_portnum(int portnum);
bool lora_utf8_valid(const uint8_t* data, size_t len);

// Parse protobuf fields -> list stored in caller-provided scratch (max fields)
struct LoraProtoField {
    uint32_t field_num;
    uint8_t wire_type;
    uint32_t varint_val;
    const uint8_t* bytes_val;
    size_t bytes_len;
};
size_t lora_parse_protobuf(const uint8_t* data, size_t len,
                           LoraProtoField* fields, size_t max_fields);

void lora_node_id_str(const uint8_t air4[4], char* out, size_t out_cap); // Meshtastic !id (byte-reversed display)

// --- High-level packet processing -------------------------------------------
LoraDecodeOutcome lora_decode_process_air(const uint8_t* raw, size_t rawLen, LoraPacket& pkt);

// Legacy API (kept for callers; prefer lora_decode_process_air)
bool lora_decode_parse_meshtastic(const uint8_t* raw, size_t rawLen, LoraPacket& pkt);
bool lora_decode_try_decrypt(uint8_t* buf, size_t len, const char* keys, int keyCountHint = 4);
void lora_decode_dispatch_telemetry(uint8_t port, const uint8_t* payload, size_t len, std::string& outInfo);
void lora_make_preview(const uint8_t* data, size_t len, char* out, size_t outCap);

bool lora_crc16_ok(const uint8_t* data, size_t len, uint16_t rxCrc);
void lora_dewhiten(uint8_t* data, size_t len, uint16_t nonce = 0);

#if defined(LORA_DECODE_HOST_TEST)
int lora_decode_run_selftests();
#endif
