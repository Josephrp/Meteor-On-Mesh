#pragma once
/*
 * ESP32-native LoRa wideband DSP (vendored from Lora-Wideband-Decoder logic).
 *
 * Provides:
 *  - generate_downchirp (table)
 *  - schmidl_cox_detect (multi-lag autocorrelation style)
 *  - dechirp_find_peak (CFO + symbol energy)
 *  - process_iq_burst (top level: energy -> SC -> dechirp path)
 *
 * This lets the ESP32 (Mayhem MDK) perform the heavy detection + demod
 * steps on IQ originating from HackRF (or reduced-rate / burst captures),
 * instead of requiring a full Linux host Python stack.
 */

#include "lora_dsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration for one processing context.
typedef struct {
    float    center_hz;
    uint32_t search_bw_hz;   // total bandwidth we consider around center
    uint8_t  sf_list[6];     // e.g. {7,8,9,10,11,12}, 0-terminated or fixed
    uint8_t  num_sf;
    uint32_t bw_list[3];     // {125000,250000,500000}
    uint8_t  num_bw;
    float    sc_threshold;   // normalized SC confidence threshold
    uint16_t max_symbols;    // how many symbols to demod after detect
} lora_dsp_cfg;

// Initialize / reset tables for common SF/BW.
void lora_dsp_init(void);

// Generate (or fetch cached) downchirp for given sf,bw,fs. Returns pointer to internal table.
const lora_cpx_f32* lora_dsp_get_downchirp(uint8_t sf, uint32_t bw_hz, float fs, size_t* out_len);

// Run a Schmidl-Cox style detector on a complex float buffer.
// Returns number of candidates written (up to max_cand).
int lora_dsp_schmidl_cox_detect(const lora_cpx_f32* iq, size_t len, float fs,
                                uint8_t sf, uint32_t bw_hz,
                                float threshold,
                                lora_candidate* out, int max_cand);

// Dechirp a window starting at 'start_sample', return best bin (symbol) and quality (PMR-like).
// Also estimates fractional CFO from the peak.
int lora_dsp_dechirp_peak(const lora_cpx_f32* iq, size_t len,
                          size_t start_sample,
                          uint8_t sf, uint32_t bw_hz, float fs,
                          float* out_cfo_hz, float* out_quality);

// High-level: take an IQ burst (sc16 or f32), run energy + SC + dechirp for configured SF/BW.
// Fills 'out_pkts' (simplified decoded view). Returns number of results.
// This is the "vendored heavy DSP" entry point.
int lora_dsp_process_burst(const lora_iq_burst* burst,
                           const lora_dsp_cfg* cfg,
                           lora_decoded_pkt* out_pkts, int max_pkts);

// Convenience: feed a block of sc16 samples (common from SDRs). Internally converts to f32.
int lora_dsp_feed_sc16(const int16_t* iq, size_t count, float fs, float center_hz,
                       const lora_dsp_cfg* cfg,
                       lora_decoded_pkt* out, int max_out);

#ifdef __cplusplus
}
#endif
