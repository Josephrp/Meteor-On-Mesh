#pragma once
/*
 * Simple packet feed for the LoRa Wideband Decoder app.
 * Used by the HTTP POST bridge (from vendored host LWD) to deliver packets
 * into the running EPAppLoraDecoder on the ESP32.
 */
#include <cstdint>
#include <string>

struct LoraPacket;  // forward from the app header

// Push a packet (implementation lives with the app or a thin wrapper).
// Safe to call from web handler context.
void lora_decoder_push_packet(uint32_t ts_ms,
                              float freq_mhz,
                              uint32_t bw_hz,
                              uint8_t sf,
                              int16_t rssi,
                              int8_t snr,
                              const char* payload_hex,
                              uint8_t proto,
                              const char* info);
