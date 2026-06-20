#pragma once
/*
 * Stub for direct HackRF sample acquisition on the ESP32.
 *
 * The heavy DSP (Schmidl-Cox, dechirp, soft demod) is now in lora_dsp.* and can
 * run on the ESP32 when fed IQ.
 *
 * To get IQ *from a HackRF directly into the ESP32* (no host Python), you would:
 *   - Run the ESP32 as USB host (ESP-IDF usb host stack + tinyusb or esp-usb).
 *   - Implement enough of the HackRF USB protocol (libhackrf style) to do:
 *       set_sample_rate, set_freq, set_lna_gain, set_vga_gain, set_txvga,
 *       start streaming via bulk endpoint (usually 0x81 or similar).
 *   - Receive sc8 or sc16 blocks, convert, and call lora_dsp_feed_sc16 (or f32).
 *
 * Bandwidth reality:
 *   - 20 Msps sc16 = ~80 MB/s. ESP32-S3 + PSRAM typically cannot sustain this
 *     for continuous wideband + DSP.
 *   - Practical: 2-5 Msps sc8/sc16 bursts, or let the HackRF (or PortaPack
 *     baseband) do coarse energy detection and forward only short high-value
 *     bursts to the ESP32 for full DSP.
 *
 * This stub exists so the "ESP32 + HackRF firmware processes LoRa" story is
 * complete in the tree. Fill in the USB transport later; the DSP is ready.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USB host + HackRF transport (no-op in current stub).
esp_err_t hackrf_sampler_init(void);

// Configure center, sample rate, gains. Returns 0 on success (stub).
int hackrf_sampler_configure(float center_hz, float fs, int gain_db);

// Start streaming. Samples will be delivered via a callback or ring that
// the caller drains and feeds to lora_dsp_feed_*.
int hackrf_sampler_start(void (*on_samples)(const int16_t* iq, size_t count, float fs, float center_hz));

// Stop.
void hackrf_sampler_stop(void);

#ifdef __cplusplus
}
#endif
