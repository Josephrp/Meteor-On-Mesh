# LoRa Wideband Decoder — Vendored + ESP32PP / Mayhem MDK Integration

This directory is a **comprehensive vendor** of the LoRa Wideband Decoder (originally `Lora-Wideband-Decoder-main`) into the Mayhem MDK (ESP32PP) firmware tree.

## Why it is here

The decoder is a **Linux host Python application** (SoapySDR + NumPy/SciPy + Flask). It performs wideband capture (HackRF up to ~20 Msps), Schmidl-Cox preamble detection, soft demod, CRC, AES-CTR (Meshtastic), and protocol dispatch. It cannot run on the ESP32 or PortaPack MCU.

It is vendored here so that the **entire Meshtonic H4M + HackRF + PortaPack** story lives in one firmware+tool tree, and the ESP32 MDK firmware can act as the **control surface, bridge target, and local radio monitor**.

## How the ESP32 "application" works (as implemented in Mayhem MDK)

- A first-class `EPApp` (`EPAppLoraDecoder`) registered in `AppList::LORADECODER`.
- Startable from:
  - PortaPack via appmgr + custom `PPCMD_LORADEC_*` commands.
  - ESP32 web UI (websocket commands `LORA:START`, `LORA:CONFIG:...`, `LORA:INJECT:...`, `LORA:BACKEND:2`, `LORA:FEED_TEST`).
  - Display (title + last packet summary).
- Three backends (config.backend):
  1. **Host bridge (recommended for true wideband at 20 Msps)**: Run the vendored decoder on a Linux host with HackRF. The host POSTs normalized packet events to `/lwd/packet` (or `/meshtonic/packet`). The ESP32 LoraDecoder app receives them.
  2. **Local SX1262 radios** (1–4 WIO on Meshtonic H4M): `SXRadioManager` + MCP23017 arming. Full RX needs SX126x driver work on top.
  3. **Embedded DSP (new)**: The heavy DSP (Schmidl-Cox detection, downchirp generation, dechirp+FFT CFO/symbol decision, basic packetization) has been comprehensively vendored into `main/lora_dsp/` as C/C++. Call `feedIQ_sc16(...)` (or the burst API) with samples originating from a HackRF and the ESP32 performs the processing locally. A test feeder (`LORA:FEED_TEST`) and a HackRF USB sampler stub (`hackrf_sampler.h`) are provided. Real sustained 20 Msps is bandwidth-limited on ESP32-S3; use bursts at 1-2 Msps or pre-channelized data from the HackRF/PP side.

## Running the vendored decoder against HackRF

From the `lora-wideband-decoder` directory (or its original location):

```bash
# Typical for US915 Meshtastic
python -m src.soapy_rx --driver hackrf -f 915.0e6 -s 20e6 --format sc16 \
| python src/decoder.py --config lora.toml --harness /tmp/lwd.jsonl ...
```

Then run the bridge (example included in the parent tree or original):

```bash
python meshtonic_bridge_example.py --url http://<esp-ip-or-ap>/lwd/packet --infile /tmp/lwd.jsonl
```

Or point your own post-producer at `POST /lwd/packet`.

The ESP32 will forward received events into the running LoraDecoder app instance.

## ESP32 web commands (over /ws)

- `LORA:START`
- `LORA:STOP`
- `LORA:CONFIG:center=915.0,backend=0,radio_count=2`
- `LORA:PACKETS`
- `LORA:INJECT:p=cafebabe...` (test injection)
- `LORA:STATUS`

The app pushes JSON: `{"type":"loradec_status", ...}` and `{"type":"loradec_packets", "packets":[...]}`.

## PortaPack / I2C surface

New commands (see `pp_commands.hpp`):

- `PPCMD_LORADEC_GETSTATUS`
- `PPCMD_LORADEC_GETPACKETS`
- `PPCMD_LORADEC_SETCONFIG`
- `PPCMD_LORADEC_CONTROL`

A PortaPack app can query status and a small number of recent packets and render a "LoRa Intercept / Wideband" view.

## Files of interest inside this vendored tree

- `src/decoder.py` — the actual multi-protocol soft decoder
- `src/detector.py` + `detect_pool.py` — wideband energy + preamble gate
- `src/soapy_rx.py` — HackRF / bladeRF capture
- `lora.toml` — configuration (rate, center, SF/BW list, keys, export dir in /dev/shm)
- `src/sdr_profiles.py` — device profiles (HackRF limited to 20 Msps)
- `run/headless.py`, `run/web.py`, `src/web/app.py` — orchestration and live UI

## Relationship to other Meshtonic components

- `mayhem-mdk/` (this ESP32 firmware) — sensors, GPS, I2C slave to PP, **this** LoraDecoder app + bridge target, MCP/TCA for 1-4 local radios.
- `meshtastic/` custom variant — active full Meshtastic node using the same 1-4 WIO radios (different flash image).
- Host Linux box + HackRF — runs this vendored wideband decoder for passive intercept of everything in band.

The three pieces are complementary: active mesh (Meshtastic), field UI + sensors (MDK + PP), passive wideband survey (LWD + HackRF).

## Legal / notes

Passive receive only where permitted. The decoder can decrypt traffic protected by the default/public Meshtastic key by design.

Keep the Flask UI (if used) on localhost or properly firewalled.

## Updating the vendor

When a new upstream version of the wideband decoder is desired:

1. `rm -rf lora-wideband-decoder`
2. `cp -a ../Lora-Wideband-Decoder-main lora-wideband-decoder` (or git subtree / submodule)
3. Re-test the bridge to `/lwd/packet`
4. Update this file with the new upstream commit if pinned.

This vendoring + the `EPAppLoraDecoder` makes the decoder a first-class "application" that uses the ESP32 exactly as the rest of the Mayhem MDK implements on-system apps, web, display, and PP I2C command surface.
