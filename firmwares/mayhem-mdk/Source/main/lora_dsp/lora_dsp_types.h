#pragma once
/*
 * Vendored / ported core types for on-ESP32 LoRa wideband DSP.
 * Goal: allow the ESP32 (Mayhem MDK + Meshtonic H4M) to perform
 * Schmidl-Cox detection + dechirp/FFT soft demod on IQ coming from
 * a HackRF (or other wideband source) instead of offloading everything
 * to a host Python process.
 *
 * Constraints acknowledged:
 * - Full 20 Msps continuous wideband on ESP32-S3 is not practical
 *   (RAM bandwidth + compute for repeated large FFTs).
 * - Typical use: process short bursts at 1-2 Msps (after host/PP coarse
 *   energy detection or reduced-rate capture) or narrow sub-bands.
 * - For production wideband survey at full rate, the host Python LWD
 *   (vendored alongside) remains the reference; this is the embedded
 *   DSP engine for when you want the ESP32 to do the heavy lifting.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float real;
    float imag;
} lora_cpx_f32;

typedef struct {
    int16_t i;
    int16_t q;
} lora_cpx_sc16;   // sc16 from SDRs

typedef struct {
    uint32_t ts_ms;     // device time when burst started
    float    center_hz;
    float    fs;        // sample rate of this buffer
    const void* samples; // either sc16 or f32 depending on fmt
    size_t   count;     // number of complex samples
    uint8_t  fmt;       // 0=sc16, 1=f32
} lora_iq_burst;

typedef struct {
    uint8_t  sf;        // 7..12
    uint32_t bw_hz;     // 125000, 250000, 500000
    float    fs;        // processing rate (often 2*bw or bw)
    float    freq_offset_hz; // estimated CFO
    float    confidence; // SC or PMR derived
    int16_t  rssi;      // rough, if available
} lora_candidate;

typedef struct {
    uint8_t  sf;
    uint32_t bw_hz;
    int16_t  rssi;
    int8_t   snr_q8;     // 0.25 dB units or similar
    uint8_t  payload[256];
    uint16_t payload_len;
    uint8_t  crc_ok;
    uint8_t  proto;      // 0=raw, 1=meshtastic-like framing detected
} lora_decoded_pkt;

#ifdef __cplusplus
}
#endif
