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

## Rich Native "Apps over I2C" UI (SatTrack pattern) — Recommended

For the best experience on the HackRF color screen:

- The ESP32PP registers a **MeshtonicLoRa** app via `PPHandler::add_app(...)` (binary starts with `standalone_app_info` header).
- PP discovers it via the standard module protocol (`I2cDev_PPmod` → device_info + getStandaloneAppInfo).
- User taps the icon (placed under RX); PP downloads the UI binary over the standard APP_TRANSFER path and runs it natively.
- While running, the PP-side UI uses the high-level custom commands below to fetch live data and send controls.
- All actual RF and decode happens on the Meshtonic 4× WIO SX1262 shields (WIO backend preferred). HackRF is used only for its screen + controls.

### New high-level custom commands for the rich UI (0xa02x range)

| CMD                  | Dir         | Purpose |
|----------------------|-------------|---------|
| `PPCMD_LORADEC_STATUS`  (0xa020) | PP req → ESP | Returns `lora_rich_status_t` (running, backend, radio_count, active_preset, total, mask) |
| `PPCMD_LORADEC_PACKETS` (0xa021) | PP req → ESP | Returns array of `lora_packet_compact_t` (slot/region/preset/proto/rssi/info + preview) |
| `PPCMD_LORADEC_PRESETS` (0xa022) | PP req → ESP | Returns list of `lora_preset_entry_t` from the generated presets (presets.toml is the source) |
| `PPCMD_LORADEC_APPLY`   (0xa023) | PP → ESP     | Send preset id (null-terminated) to apply immediately on the WIO radios |
| `PPCMD_LORADEC_CONTROL` (0xa024) | PP → ESP     | start/stop, clear, backend hints, etc. (byte 0 = op) |

These coexist with the older basic LORADEC commands (0xa00e–0xa013) and GETUI.

### Fallback / coexistence

- The basic EPApp commands (`GETSTATUS`, `GETPACKETS`, `CONTROL`, `GETUI`, `FEEDIQ`) continue to work for simple views and the on-ESP OLED.
- The rich app uses the new 0xa02x commands for full packet lists, preset browsing, and live status.
- When the rich app is active on the PP screen, the ESP should be in WIO (or hybrid-as-appropriate) backend and must **not** arm the HackRF RF path for LoRa demod.

## Legacy / simple PortaPack integration (still supported)

The older flow (launch EPApp 04, poll GETUI, optionally FEEDIQ) remains for basic LCD status or when no rich binary is registered.

## Implementation notes (this repo)

- `pp_commands.hpp` — command IDs
- `ppi2c/pp_structures.hpp` — `standalone_app_info`, `lora_*_t` compact records
- `main.cpp` — registration of app binary + `add_custom_command` send/got callbacks
- `apps/ep_app_loradecoder.*` + `lora_decoder_feed.h` — data providers and `lora_rich_apply_preset`
- `extapps/meshtonic_lora.h` — placeholder binary (header + tiny body). Replace with a real built image from the Mayhem firmware tree following the SatTrack example.

Reference real pattern: `extapps/sattrack.h`, registration + callbacks in `main.cpp`, `I2cDev_PPmod` on the PP side.

## Building the real rich UI binary (PortaPack Mayhem firmware side)

1. In the Mayhem repo, create `firmware/standalone/meshtonic_lora/` (or a minimal module app) that:
   - Begins with a correct `standalone_app_info` at the image front.
   - Uses the PP UI framework (Painter, widgets, encoder) to render packet list / slot bars / preset picker.
   - On a timer or button, issues the 0xa02x custom commands via the module I2C path to fetch data from the ESP.
   - On user action (choose preset, start/stop) sends APPLY / CONTROL.
2. Build the image, convert to C array (e.g. `xxd -i meshtonic_lora.bin > meshtonic_lora.h`), and drop into this repo's `Source/extapps/`.
3. Rebuild the ESP firmware — the app will now appear and run as a first-class citizen on the HackRF screen.

All RF work stays on the WIO shields. The onboard HackRF RF is not used for LoRa in this app.
