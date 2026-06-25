#pragma once

/*
 * Meshtonic H4M Companion v2 — WIO1-first Meshtastic port.
 *
 * Verified against pcb/meshtonic_h4m_v2.kicad_pcb and the official XIAO ESP32-S3
 * Plus D->GPIO map (Dn != GPIOn above D5). For WIO1: CS and DIO1 are NATIVE XIAO
 * GPIOs; BUSY is on the MCP23017; LORA_RST is a shared MCP line. SPI data lines
 * are native GPIOs routed via the ESP32-S3 GPIO matrix.
 */

#define MESHTONIC_H4M 1
#define MESHTONIC_H4M_MCP_RADIO 1

#define HW_VENDOR meshtastic_HardwareModel_PRIVATE_HW

#define BUTTON_PIN 21
#define BUTTON_NEED_PULLUP

#define I2C_SDA 5
#define I2C_SCL 6

#define BATTERY_PIN -1
#define ADC_CHANNEL ADC_CHANNEL_0

/* GPS — NEO-6M on Grove J4 / UART1 (D8/D9) */
#define HAS_GPS 1
#define GPS_UBLOX 1
#define GPS_RX_PIN 7
#define GPS_TX_PIN 8
#define GPS_THREAD_INTERVAL 50

/* Shared SPI (radio + TFT + optional SD) — XIAO Plus pads D11/D12/D13 = GPIO38/39/40 */
#define LORA_SCK 40
#define LORA_MISO 39
#define LORA_MOSI 38

/* Optional microSD on D15/GPIO42 — enable when socket populated and SDFs is configured in platformio.ini */
/* #define HAS_SDCARD 1 */
/* #define SDCARD_CS 42 */

/* WIO1 control (v2): CS + DIO1 are native XIAO GPIOs; BUSY is on the MCP23017.
 * (XIAO ESP32-S3 Plus: D2=GPIO3, D18=GPIO12, D7=GPIO44.) */
#define MESHTONIC_MCP23017_ADDR 0x20
#define MESHTONIC_WIO1_CS_GPIO 3     /* native D2 — strapping pin, output idle-high */
#define MESHTONIC_WIO1_DIO1_GPIO 12  /* native D18 */
#define MESHTONIC_MCP_WIO1_BUSY 0    /* MCP GPA0 */
#define MESHTONIC_MCP_LORA_RST 12    /* MCP GPB4 — shared across all radios */
#define MESHTONIC_MCP_INTA_PIN 44    /* native D7/GPIO44 */

/* TCA9548A @ 0x70 — sensor hub channels 0-3 */
#define MESHTONIC_TCA9548A_ADDR 0x70

/*
 * Virtual RadioLib pins — routed through MCP23017 via meshtonic_radio_gpio.cpp.
 * Pattern matches STM32WL virtual GPIO numbers in Meshtastic.
 */
#define MESHTONIC_VPIN_CS 1000
#define MESHTONIC_VPIN_DIO1 1001
#define MESHTONIC_VPIN_RST 1002
#define MESHTONIC_VPIN_BUSY 1003

#define USE_SX1262 1

#define SX126X_CS MESHTONIC_VPIN_CS
#define SX126X_DIO1 MESHTONIC_VPIN_DIO1
#define SX126X_RESET RADIOLIB_NC
#define SX126X_BUSY MESHTONIC_VPIN_BUSY

#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL

#define LORA_CS SX126X_CS
#define LORA_DIO1 SX126X_DIO1
#define LORA_RESET SX126X_RESET

/* ILI9341 TFT on shared SPI */
#define HAS_SCREEN 1
#define HAS_TFT 1

#define SPI_FREQUENCY 40000000
#define LGFX_DRIVER_TEMPLATE
#define LGFX_DRIVER LGFX_GENERIC
#define GFX_DRIVER_INC "graphics/LGFX/LGFX_GENERIC.h"
#define LGFX_PANEL ILI9341
#define LGFX_SCREEN_WIDTH 240
#define LGFX_SCREEN_HEIGHT 320
#define DISPLAY_SIZE 240x320
#define LGFX_PIN_SCK 40
#define LGFX_PIN_MOSI 38
#define LGFX_PIN_MISO 39
#define LGFX_PIN_DC 43
#define LGFX_PIN_CS 11
#define LGFX_PIN_RST 9
#define LGFX_PIN_BL -1
#define LGFX_ROTATION 0

#define ENVIRONMENTAL_TELEMETRY_MODULE_ENABLE 1
