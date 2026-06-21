# Meshtonic H4M — frozen pin audit (WIO1 target)

Sources: `pcb/meshtonic.xml`, `pcb/Xiao_Core.kicad_sch`, Seeed XIAO ESP32-S3 Plus module docs.

| Board D | XIAO pad | Net | GPIO | Usage |
|---------|----------|-----|------|-------|
| D4 | 5 | I2C_SDA | 5 | Main I2C (TCA9548A, MCP23017, AS5600) |
| D5 | 6 | I2C_SCL | 6 | Main I2C |
| D6 | 7 | TFT_DC | 43 | ILI9341 D/C |
| D8 | 9 | UART1_RX | 7 | GPS NEO-6M RX |
| D9 | 10 | UART1_TX | 8 | GPS NEO-6M TX |
| D10 | 11 | TFT_RST | 9 | ILI9341 RESET |
| D11 | 15 | SPI_MOSI | 11 | Shared SPI |
| D12 | 16 | SPI_MISO | 12 | Shared SPI |
| D13 | 17 | SPI_SCK | 13 | Shared SPI |
| D14 | 18 | ADC_LIGHT | 39 | TEMT6000 (deferred in firmware) |
| D15 | 19 | CS_SD | 38 | microSD CS |
| D16 | 20 | MCP_INTA | 10 | MCP23017 INTA → radio IRQ via MCP |
| D17 | 23 | CS_TFT | 17 | ILI9341 CS (pad#23 audit: GPIO17) |

## WIO1 SX1262 control (MCP23017 @ 0x20)

| MCP pin | Function |
|---------|----------|
| GPA3 | CS_WIO1 (output, idle high) |
| GPA5 | WIO1_DIO1 (input) |
| GPA7 | WIO1_BUSY (input; verify errata on fitted MCP rev) |
| INTA | To D16 / GPIO10 |

`LORA_RST` is a passive shared net (no MCU/MCP driver). Firmware uses RadioLib soft reset (`SX126X_RESET = RADIOLIB_NC`).

## TCA9548A @ 0x70

| Channel | Sensor |
|---------|--------|
| 0 | BMM150 |
| 1 | BMP280 |
| 2 | SHT3x |
| 3 | BMI160 |

AS5600 @ 0x36 and MCP23017 @ 0x20 remain on the main bus.
