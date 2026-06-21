# Meshtonic H4M WIO1 — Meshtastic variant

Board-specific Meshtastic firmware for Meshtonic H4M Companion v2 with a single populated WIO1 (SX1262) module.

## Files

| File | Role |
|------|------|
| `pins_arduino.h` | Authoritative D0–D19 → GPIO map for this PCB |
| `variant.h` | Meshtastic feature flags, SPI/UART/GPS/TFT/LORA defines |
| `meshtonic_mcp23017.*` | MCP23017 driver for WIO1 CS/DIO1/BUSY |
| `meshtonic_radio_gpio.*` | RadioLib virtual GPIO + MCP INTA interrupt |
| `meshtonic_tca9548a.*` | TCA9548A channel select + muxed sensor discovery |
| `meshtonic_board.cpp` | Early init hook from `main.cpp` |
| `src/mesh/meshtonic/*.cpp` | Implementation (compiled only for this env) |
| `platformio.ini` | `env:meshtonic-h4m-wio1` build target |
| `PIN_AUDIT.md` | Frozen pin audit notes |

## Radio

- SPI: MOSI/MISO/SCK = GPIO 11/12/13 (direct XIAO pads)
- CS, DIO1, BUSY: MCP23017 GPA3/GPA5/GPA7
- IRQ: MCP INTA on GPIO10 (D16)
- Reset: passive net — RadioLib soft reset only (`SX126X_RESET = RADIOLIB_NC`)

## Sensors (TCA9548A)

Main bus: MCP23017, AS5600. Mux channels 0–3: BMM150, BMP280, SHT3x, BMI160.

Upstream `ScanI2CTwoWire` is patched to walk the mux at scan time and select the correct channel before I2C transactions.

## Coexistence

This tree lives under `firmwares/meshtastic/` beside other firmware projects (PortaPack Havoc, standalone apps). It does not modify upstream board definitions except via the isolated patches guarded by `MESHTONIC_H4M` / `MESHTONIC_H4M_MCP_RADIO`.
