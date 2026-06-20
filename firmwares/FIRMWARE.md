# Meshtonic H4M — Meshtastic Custom Firmware

Custom board-specific Meshtastic port for the Meshtonic H4M Companion v2 (WIO1-first bring-up).

## Upstream pin

Meshtastic firmware is vendored at `firmwares/meshtastic/` (commit in `MESHTASTIC_COMMIT`).

## Build

```bash
cd firmwares/meshtastic
python -m platformio run -e meshtonic-h4m-wio1
```

Artifacts (after a successful build):

- `.pio/build/meshtonic-h4m-wio1/firmware-meshtonic-h4m-wio1-2.8.0.161cd26.bin` — OTA/app partition
- `.pio/build/meshtonic-h4m-wio1/firmware-meshtonic-h4m-wio1-2.8.0.161cd26.factory.bin` — full factory image @ offset 0
- `.pio/build/meshtonic-h4m-wio1/littlefs-meshtonic-h4m-wio1-2.8.0.161cd26.bin` — LittleFS map/UI assets

Build validated on Windows (2026-06-20): ~16 min with LTO path warnings that are ignored upstream; link completes successfully.

Flash:

```bash
pio run -e meshtonic-h4m-wio1 -t upload
pio device monitor -b 115200
```

## Why custom code exists

| Hardware quirk | Custom shim |
|----------------|-------------|
| WIO1 CS / DIO1 / BUSY on MCP23017 @ 0x20 | `meshtonic_mcp23017.*`, `meshtonic_radio_gpio.*`, RadioLib GPIO callbacks |
| Radio IRQ via MCP INTA → D16 / GPIO10 | `meshtonicMcpEnableRadioInterrupt()` |
| Sensors behind TCA9548A @ 0x70 | `meshtonic_tca9548a.*`, ScanI2C hooks |
| Board D-labels ≠ stock XIAO Plus Arduino map | `pins_arduino.h` in this variant (never use stock Plus mapping) |

## Validation checklist (hardware)

1. Serial log: MCP23017 + TCA9548A init OK
2. I2C scan: MCP @ 0x20, TCA @ 0x70, AS5600 @ 0x36; mux ch0–3 sensors after channel walk
3. SX1262 init success (WIO1 only)
4. GPS NMEA on UART1 (GPIO7/8)
5. ILI9341 UI on shared SPI
6. LoRa TX/RX with a second Meshtastic node

## Deferred

- **LD2450** — shares UART1 with GPS
- **TEMT6000** — analog on D14/GPIO39; no stock Meshtastic telemetry module

See `variants/esp32s3/diy/meshtonic_h4m_wio1/PIN_AUDIT.md` for the frozen GPIO table.
