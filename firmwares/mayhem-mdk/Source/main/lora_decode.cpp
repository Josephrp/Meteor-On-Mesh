#include "lora_decode.h"
#include "apps/ep_app_loradecoder.hpp" // for LoraPacket layout
#include <cstdio>
#include <stdio.h>
#include <cstring>
#include <cctype>

// Simple Meshtastic air format (post decrypt, pre any extra crc):
// to(4) from(4) id(4) flags(1) port(1) [payload]  (total len often in header or implicit)
// For onboard we receive the bytes after SX hardware (no whitening in many configs).

static inline uint16_t crc16_ccitt(const uint8_t* d, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i=0; i<n; i++) {
        crc ^= (uint16_t)d[i] << 8;
        for (int b=0; b<8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void lora_dewhiten(uint8_t* data, size_t len, uint16_t /*nonce*/) {
    // SX1262 hardware dewhitens for standard LoRa; for some explicit use a LFSR.
    // No-op for common Meshtastic private sync cases.
    (void)data; (void)len;
}

bool lora_crc16_ok(const uint8_t* data, size_t len, uint16_t rxCrc) {
    if (len < 2) return true; // trust hw
    uint16_t calc = crc16_ccitt(data, len-2);
    return calc == rxCrc;
}

bool lora_decode_parse_meshtastic(const uint8_t* raw, size_t rawLen, LoraPacket& pkt) {
    if (!raw || rawLen < 10) return false;
    // Heuristic: assume first bytes are to/from if looks like node ids
    uint32_t to = (uint32_t)raw[0] | (raw[1]<<8) | (raw[2]<<16) | (raw[3]<<24);
    uint32_t from = (uint32_t)raw[4] | (raw[5]<<8) | (raw[6]<<16) | (raw[7]<<24);
    uint8_t flags = raw[8];
    uint8_t port = raw[9];
    (void)port;
    (void)rawLen;

    char info[128];
    snprintf(info, sizeof(info), "from:%08lx to:%08lx p:%u f:%02x",
             (unsigned long)from, (unsigned long)to, port, flags);
    pkt.info = info;
    pkt.proto = 1; // meshtastic

    // preview payload hex already in caller; here we could truncate
    return true;
}

bool lora_decode_try_decrypt(uint8_t* buf, size_t len, const char* keys, int /*hint*/) {
    // Stub: for demo we accept plaintext or first key=="default" does nothing.
    // Full AES-CTR port would use mbedtls or tiny aes here with key retry loop.
    // If keys provided and look like hex try simple XOR with first 16 as demo key.
    if (!keys || !*keys) return true;
    // demo: if key starts with '!' treat as bypass
    if (keys[0] == '!') return true;
    // Otherwise leave as-is (real decrypt would mutate buf)
    return true;
}

void lora_decode_dispatch_telemetry(uint8_t port, const uint8_t* payload, size_t len, std::string& outInfo) {
    // Common MT ports: 1=text, 3=pos, 4=userinfo, 5=tele etc. Minimal parse.
    if (port == 1 && len > 0) {
        outInfo += " text";
    } else if (port == 3) {
        outInfo += " pos";
    } else if (port == 4) {
        outInfo += " nodeinfo";
    } else if (port == 5 || port == 67) {
        outInfo += " metrics";
    }
}

void lora_make_preview(const uint8_t* data, size_t len, char* out, size_t outCap) {
    if (!out || outCap==0) return;
    size_t n = (len < outCap/2-1) ? len : outCap/2-1;
    for (size_t i=0; i<n; i++) {
        sprintf(out + i*2, "%02x", data[i]);
    }
    out[n*2] = 0;
}