# Meshtonic H4M v2 — pin audit (WIO1 target)

Sources: `pcb/meshtonic_h4m_v2.kicad_pcb` netlist, `pcb/Xiao_Core.kicad_sch`, and
the official Espressif `XIAO_ESP32S3_Plus` `pins_arduino.h` + Seeed wiki.

> **Correction vs. the old v1 audit (now in `pcb/refs/v1`):** on the XIAO ESP32-S3
> **Plus**, `Dn != GPIOn` above D5 — the mapping is irregular
> (`D11=GPIO38 … D19=GPIO11`). The earlier audit assumed `Dn = GPIOn` and was wrong
> for SPI and for D16–D19. The GPIO column below is the **real silicon GPIO**.

| Board D | XIAO pad | Net | GPIO | Usage |
|---------|----------|-----|------|-------|
| D0 | 1 | UART0_TX | 1 | (console on USB; UART0 free) |
| D1 | 2 | UART0_RX | 2 | |
| D2 | 3 | CS_WIO1 | 3 | WIO1 chip-select (native; **strapping pin**, idle high) |
| D3 | 4 | CS_WIO2 | 4 | WIO2 chip-select (native) |
| D4 | 5 | I2C_SDA | 5 | Main I2C (TCA9548A, MCP23017, AS5600) |
| D5 | 6 | I2C_SCL | 6 | Main I2C |
| D6 | 7 | TFT_DC | 43 | ILI9341 D/C (UART0 TXD repurposed) |
| D7 | 8 | MCP_INTA | 44 | MCP23017 INTA → radio IRQ (UART0 RXD repurposed) |
| D8 | 9 | UART1_RX | 7 | GPS NEO-6M RX |
| D9 | 10 | UART1_TX | 8 | GPS NEO-6M TX |
| D10 | 11 | TFT_RST | 9 | ILI9341 RESET |
| D11 | 15 | SPI_MOSI | 38 | Shared SPI |
| D12 | 16 | SPI_MISO | 39 | Shared SPI (JTAG MTCK) |
| D13 | 17 | SPI_SCK | 40 | Shared SPI (JTAG MTDO) |
| D14 | 18 | ADC_LIGHT | 41 | TEMT6000 — **digital-only pin, not ADC-capable** (deferred) |
| D15 | 19 | CS_SD | 42 | microSD CS (JTAG MTMS) |
| D16 | 20 | CS_WIO3 | 10 | WIO3 chip-select (native) |
| D17 | 23 | CS_WIO4 | 13 | WIO4 chip-select (native) |
| D18 | 22 | WIO1_DIO1 | 12 | WIO1 DIO1 (native) |
| D19 | 21 | CS_TFT | 11 | ILI9341 chip-select (native) |

## SX1262 radio control (MCP23017 @ 0x20)

Chip-selects are native ESP32-S3 GPIOs (above); the MCP carries per-radio status
inputs and shared control outputs.

| MCP pin | Function |
|---------|----------|
| GPA0 | WIO1_BUSY (input) |
| GPA1 | WIO2_BUSY (input) |
| GPA2 | WIO3_BUSY (input) |
| GPA3 | WIO4_BUSY (input) |
| GPA4 | WIO2_DIO1 (input) |
| GPA5 | WIO3_DIO1 (input) |
| GPA6 | WIO4_DIO1 (input) |
| GPB0 | WIO_RF_SW_EN (output) |
| GPB1 | CS_GROVE (output) |
| GPB4 | LORA_RST (output, **shared across all 4 radios**) |
| GPB5 | LED_WIO1 (output) |
| GPB6 | LED_WIO2 (output) |
| GPB7 | LED_WIO3 (output) |

WIO1 DIO1 is native (GPIO12), not on the MCP. INTA → native GPIO44 (D7).
`LORA_RST` is a single shared net; firmware does not pulse it per-radio
(`SX126X_RESET = RADIOLIB_NC`, soft standby per slot).

## TCA9548A @ 0x70

| Channel | Sensor |
|---------|--------|
| 0 | BMM150 |
| 1 | BMP280 |
| 2 | SHT3x |
| 3 | BMI160 |

AS5600 @ 0x36 and MCP23017 @ 0x20 remain on the main bus.
