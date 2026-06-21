# PortaPack LoRa Decoder — On-Device Protocol (no host PC)

The Meshtonic H4M runs the **LoRa Decoder** EPApp on the ESP32 at boot. When a PortaPack/HackRF is attached over I2C, the ESP32 processes IQ bursts on-chip and shows status on **both** the ESP OLED and the PortaPack LCD.

## Architecture

```
HackRF (in PortaPack)  →  energy detect + short IQ burst
        ↓ I2C (PP CMD 0xA012)
ESP32-S3               →  lora_dsp (Schmidl-Cox + dechirp) + lora_decode (multi-protocol)
        ↓
ESP OLED + PP LCD      →  decoded packets, preset, band stats
        ↑
4× WIO SX1262          →  narrowband multi-slot listen (parallel, no PC)
```

No computer is required. Optional WiFi web UI is for remote monitoring only.

## Start the app on PortaPack

1. Launch ESP app **04** (LoRa Decoder) via App Manager, or rely on Meshtonic H4M auto-start.
2. PortaPack polls `PPCMD_LORADEC_GETUI` (0xA013) at ~2 Hz to paint its screen.
3. PortaPack wideband path: capture 1–2 Msps burst (~2–8 ms), send via `PPCMD_LORADEC_FEEDIQ` (0xA012).

## I2C commands (ESP32 slave 0x51)

| CMD | Dir | Purpose |
|-----|-----|---------|
| `0xA00E` | PP→ESP req | GETSTATUS — running, backend, center, slot mask |
| `0xA00F` | PP→ESP req | GETPACKETS — last 4 packets (compact binary) |
| `0xA010` | PP→ESP data | SETCONFIG — center_mhz, radio_count, backend |
| `0xA011` | PP→ESP data | CONTROL — see below |
| `0xA012` | PP→ESP data | FEEDIQ — HackRF IQ burst chunks |
| `0xA013` | PP→ESP req | GETUI — 6×20 char lines for PP LCD |

### CONTROL (0xA011) byte 0

| Op | Action |
|----|--------|
| 0 | STOP |
| 1 | START |
| 2 | Arm WIO radios |
| 3 | Disarm WIO radios |
| 4 | Backend = HackRF DSP only |
| 5 | Backend = WIO only |
| 6 | Backend = hybrid (WIO + HackRF) |
| 7 | Apply preset — bytes 1…N = preset id ASCII (e.g. `US915-meshtastic`) |

### FEEDIQ (0xA012) payload

| Sub | Bytes | Meaning |
|-----|-------|---------|
| 0 | 1+4+4+4 | START: `float center_mhz`, `float fs_hz`, `uint32_t sample_count` |
| 1 | 1+4N | CHUNK: little-endian sc16 IQ pairs (I,Q,I,Q,…) |
| 2 | 0 | FLUSH — process partial buffer |

Max burst: **4096** complex samples per transaction (~8 ms @ 2 Msps). PortaPack should:

1. Tune HackRF to preset center (e.g. 915 MHz US).
2. Run coarse energy / max-hold on PP firmware.
3. On trigger, capture burst at **1–2 Msps sc16**, send START + CHUNK(s) + FLUSH.
4. ESP32 runs `lora_dsp_feed_sc16` + `lora_decode_process_air_ex`, pushes packets to both displays.

### GETUI (0xA013) response

| Offset | Field |
|--------|-------|
| 0 | running (0/1) |
| 1 | backend (0=bridge, 1=wio, 2=hackrf, 3=hybrid) |
| 2 | packet count (mod 256) |
| 3 | PortaPack linked (0/1) |
| 4 | active WIO radio count |
| 5 | HackRF burst count (mod 256) |
| 6–125 | six lines × 20 ASCII chars (space padded) |

## Backends

| ID | Name | Use |
|----|------|-----|
| 1 | `wio` | Standalone H4M — 4× SX1262 only |
| 2 | `hackrf` | PortaPack only — IQ bursts |
| 3 | `hybrid` | Both (default when PP connects) |
| 0 | `bridge` | Optional WiFi ingest from host (dev only) |

ESP web/serial: `LORA:BACKEND:hybrid`, `LORA:PRESET:US915-meshtastic`, `LORA:START`.

## PortaPack app TODO (upstream PP firmware)

Implement a Mayhem/PortaPack app that:

- Shows GETUI lines on the HackRF screen (title, preset, last packet).
- Draws a simple spectrum/waterfall from HackRF baseband.
- On energy trigger, ships IQ via FEEDIQ.
- Maps encoder buttons to CONTROL ops (preset cycle, start/stop).

Reference: `Source/main/apps/ep_app_loradecoder.cpp`, `pp_commands.hpp`.
