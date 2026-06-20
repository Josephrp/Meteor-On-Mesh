#pragma once

/*
 * C++ port of core LoRa/Meshtastic decode logic from vendored LWD (decoder.py + config).
 * Focus: header parse, CRC, whitening bypass note (SX1262 hardware), AES-CTR key retry stub,
 * lightweight telemetry dispatch for common ports (text, position, nodeinfo, telemetry).
 *
 * This enriches raw LoraPacket from onboard (or host bridge) without requiring full nanopb.
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <cstdint> // ensure for clangd outside IDF
#include <cstddef>

struct LoraPacket; // from ep_app

// Attempt to parse a raw payload as Meshtastic (after possible decrypt).
// Returns true if header looks valid (len, crc ok or ignored for hw crc).
bool lora_decode_parse_meshtastic(const uint8_t* raw, size_t rawLen, LoraPacket& pkt /* enriched in place */);

// Try AES-CTR decrypt with provided key list (null terminated or count).
// On success replaces pkt.payload_hex with decrypted and sets info.
bool lora_decode_try_decrypt(uint8_t* buf, size_t len, const char* keys /* csv or space */, int keyCountHint = 4);

// Very light telemetry dispatch for known ports (no full proto).
void lora_decode_dispatch_telemetry(uint8_t port, const uint8_t* payload, size_t len, std::string& outInfo);

// Convenience: dewhiten (if software whitening was used; often SX bypasses for private).
void lora_dewhiten(uint8_t* data, size_t len, uint16_t nonce = 0);

// CRC16 check (CCITT or Meshtastic variant).
bool lora_crc16_ok(const uint8_t* data, size_t len, uint16_t rxCrc);

// Fill a preview string for display/web.
void lora_make_preview(const uint8_t* data, size_t len, char* out, size_t outCap);