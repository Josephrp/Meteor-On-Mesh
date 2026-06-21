# LoRa Wideband Decoder — Vendored + ESP32PP / Mayhem MDK Integration

This directory is a **comprehensive vendor** of the LoRa Wideband Decoder (LWD) into the Mayhem MDK (ESP32PP) firmware tree.

## Architecture (on-device first — no PC required)

| Layer | Component | Role |
|---|---|---|
| ESP32 EPApp | `ep_app_loradecoder.cpp` | Standalone app on Meshtonic OLED; auto-starts on H4M |
| 4× WIO SX1262 | `lora_radio` + `lora_decode.cpp` | Narrowband multi-band listen + full C++ decode |
| HackRF + PortaPack | I2C `PPCMD_LORADEC_FEEDIQ` | IQ bursts → ESP32 `lora_dsp` + decode (see `docs/PORTAPACK_LORADEC.md`) |
| Presets | `presets.toml` → `lora_bands.h` | Single source for region/profile PHY |
| Optional host | Python LWD + bridge | Dev/lab only — **not required** for field use |

## Backends (`config.backend`)

1. **WIO (1)** — 4× SX1262 on Meshtonic H4M; default standalone mode
2. **HackRF DSP (2)** — PortaPack sends IQ bursts over I2C; ESP32 processes on-chip
3. **Hybrid (3)** — WIO narrowband + HackRF wideband bursts (auto when PortaPack connects)
4. **Host bridge (0)** — optional WiFi POST from a PC (development only)

## Registered band presets (29)

Generated from `presets.toml`. Examples:

| Preset | Region | PHY | Use |
|---|---|---|---|
| `US915-meshtastic` | US915 | SF11/250k @ 906.875+ | Default H4M US listen |
| `US-meshcore` | US915 | SF7/62.5k @ 910.525+ | MeshCore narrow |
| `US915-lorawan` | US915 | LoRaWAN grid 902.3+ | LoRaWAN monitor |
| `EU868-meshtastic` | EU868 | 869.525 | EU Meshtastic |
| `EU-meshcore` | EU868 | 869.525/869.618 | EU MeshCore |
| `CN470-meshtastic` | CN470 | 486.3+ | China |
| … | … | … | See `LORA:PRESETS` |

Regenerate C headers after editing presets:

```bash
python firmwares/mayhem-mdk/tools/gen_lora_bands.py
```

## ESP32 web commands (`LORA:` prefix, app id 04)

| Command | Effect |
|---|---|
| `LORA:START` / `STOP` | Session on/off |
| `LORA:PRESET:US915-meshtastic` | Apply 4-slot preset + re-arm |
| `LORA:REGION:EU868` | Shorthand region preset |
| `LORA:SLOT:3=US-meshcore` | Single slot preset |
| `LORA:PRESETS` | List preset IDs (JSON) |
| `LORA:CHLIST:0=906.875,11,250000,5,cad,US915-meshtastic` | Manual PHY + optional preset |
| `LORA:DECODE:mode=cpp\|python\|auto` | Decode tier |
| `LORA:DECODE:sidecar=http://host:8765` | Python sidecar URL |
| `LORA:KEYS:…` / `LORA:KEYS:sync` | Key management / export JSON |
| `LORA:STATUS` / `PACKETS` | Push JSON status / packets |

## JSON contract (`loradec_status` / `loradec_packets`)

Packets include: `slot`, `region`, `profile`, `band`, `proto`, `confidence`, `decrypted`, `decode_backend`.

Status includes `bands_in_use[]` with per-protocol counts and `presets_available`.

## Host workflows

### Wideband + bridge

```bash
cd lora-wideband-decoder
python run/meshtonic_listener.py --preset US915-meshtastic --mode wideband --esp http://192.168.4.1
# Or manually:
python -m src.soapy_rx --driver hackrf -f 915.0e6 -s 20e6 --format sc16 \
  | python src/decoder.py --harness /tmp/lwd.jsonl
python meshtonic_bridge_example.py --url http://192.168.4.1/lwd/packet --infile /tmp/lwd.jsonl
```

### Narrowband Python decode (sidecar)

```bash
python run/narrowband_service.py --esp http://192.168.4.1 --preset US915-meshtastic
# On ESP: LORA:DECODE:mode=python
```

### Decode test vector

```bash
python tools/decode_payload.py --preset US915-meshtastic --hex "ff..."
python firmwares/mayhem-mdk/tools/equiv_harness.py
```

## HTTP endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/lwd/packet` | POST | Full `LoraDecodedRecord` JSON from host bridge |
| `/lwd/presets` | GET | List all band presets (JSON) |
| `/meshtonic/packet` | POST | Alias for `/lwd/packet` |

## Key store

- ESP NVS: `lora_keys` blob (structured entries with `protocol` field)
- Host: `lora-wideband-decoder/lora_keys.json` (Meshtastic + MeshCore defaults)
- Sync: `LORA:KEYS:sync` exports ESP key list as JSON

## Protocols decoded

| Protocol | Python LWD | On-device C++ |
|---|---|---|
| Meshtastic | Full AES-CTR + protobuf | Full |
| MeshCore | Full (Ed25519, channel decrypt) | Structural sniff |
| LoRaWAN | MHDR + grid + behavioral | Structural + grid |
| LoRaMesher, APRS, Reticulum, disaster.radio, RadioHead, EByte | Full | Structural sniff |

## Hardware

- WIO modules are band-matched (433 / 868 / 915). Match preset region to installed modules.
- Recommended US H4M: slots 0–2 `US915-meshtastic`, slot 3 `US-meshcore`.
- CAD RX policy recommended for 4-radio power budget.

## Files

- `src/lora/` — presets loader, LoRaWAN grid (all regions)
- `src/narrowband_dispatch.py` — payload-only decode API
- `presets.toml` — band registry
- `tools/gen_lora_bands.py` — generates firmware `lora_bands.h/.cpp`

## Updating the vendor

1. Replace `lora-wideband-decoder/` from upstream
2. Re-run `tools/gen_lora_bands.py` if presets changed
3. Re-test bridge + `equiv_harness.py`

Passive receive only where permitted.
