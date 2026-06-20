# lora_dsp — Vendored Heavy DSP for ESP32 LoRa Processing (Meshtonic onboard antennas)

This directory contains the **C/C++ port** of the core wideband LoRa DSP algorithms (Schmidl-Cox style detection, dechirp/FFT, etc.) from the Python Lora-Wideband-Decoder.

On Meshtonic H4M the primary receivers are the board's own 1-4 Wio-SX1262 modules + antennas (controlled via MCP23017 + shared SPI). See `../lora_radio.*`.

The DSP here can still be used for raw-IQ bursts (test / external capture). For normal operation the SX1262 chips perform the physical-layer work (CAD for detection, internal demod), and the ESP32 software layer (LoraDecoder app + vendored decode logic) does the higher-level processing.

## What was vendored / implemented

- `lora_dsp_types.h` — common structures (bursts, candidates, decoded packets)
- `lora_dsp.h / .cpp`
  - Downchirp generation (phase-accurate port of the Python version)
  - `lora_dsp_schmidl_cox_detect` — frequency-agnostic autocorrelation at symbol lag + sliding 6-symbol window + normalization + peak picking (multi-candidate, basic NMS)
  - `lora_dsp_dechirp_peak` — multiply by downchirp, FFT, argmax for symbol + quality (PMR), CFO from bin
  - `lora_dsp_process_burst` / `feed_sc16` — top-level pipeline that runs energy/SC/dechirp over configured SF/BW list and emits `lora_decoded_pkt`
- A small portable radix-2 FFT (easy to replace with `esp-dsp` `dsps_fft2r_fc32` later)
- `hackrf_sampler.h` — stub interface for direct USB host acquisition from a HackRF into the ESP32

## Integration

Used by `EPAppLoraDecoder` when `backend == 2`:

- `LORA:BACKEND:2` from the web UI
- `feedIQ_sc16(...)` from web bridge, test feeder (`LORA:FEED_TEST`), or future USB task
- Results appear in the same packet ring as the host bridge and are visible on web + PortaPack I2C

## HackRF + ESP32 reality

- The DSP algorithms now run on the ESP32.
- Getting sustained high-rate IQ from a HackRF into the ESP32 is a transport problem:
  - Direct USB host on ESP32-S3 is possible but complex (usb host stack + HackRF protocol).
  - 20 Msps sc16 (~80 MB/s) is beyond practical sustained rates on this MCU + memory system for continuous wideband.
  - Recommended practical modes:
    - Reduced sample rate on the HackRF (2–5 Msps).
    - Burst capture: HackRF (or PortaPack baseband) detects energy and forwards only short valuable records (~10k–100k samples) to the ESP32.
    - Host-side pre-processing that sends already channelized or detected bursts over WiFi/serial.

The Python LWD (in the sibling `lora-wideband-decoder/` dir) remains the gold standard for full-rate wideband survey.

## Next steps for full "ESP32 does the LoRa from HackRF"

1. Implement the USB host sampler behind `hackrf_sampler.h` (or a WiFi/TCP burst uploader).
2. Wire the sampler callback to `EPAppLoraDecoder::feedIQ_sc16` (or a global ring the app drains).
3. Optionally switch the FFT to `espressif/esp-dsp` for better performance.
4. Extend the symbol walker + dewhitening/CRC/AES path for full Meshtastic packet decode (the current impl emits a skeleton that the higher app layer can refine).

This directory + the app + the Python vendoring together give a complete story: you can run the heavy DSP on a host, or on the ESP32 itself when you have samples.
