#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

/*
 * Meshtonic H4M — board-specific pin map.
 * Derived from pcb/meshtonic.xml + Xiao_Core.kicad_sch.
 * Do NOT use stock XIAO_ESP32S3_Plus Arduino mappings.
 */

#define USB_VID 0x2886
#define USB_PID 0x0063

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t TX1 = 8;
static const uint8_t RX1 = 7;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS = 38;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 12;
static const uint8_t SCK = 13;

static const uint8_t MOSI1 = 11;
static const uint8_t MISO1 = 12;
static const uint8_t SCK1 = 13;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A8 = 7;
static const uint8_t A9 = 8;
static const uint8_t A10 = 9;
static const uint8_t ADC_BAT = 10;

static const uint8_t D0 = 1;
static const uint8_t D1 = 2;
static const uint8_t D2 = 3;
static const uint8_t D3 = 4;
static const uint8_t D4 = 5;
static const uint8_t D5 = 6;
static const uint8_t D6 = 43;
static const uint8_t D7 = 44;
static const uint8_t D8 = 7;
static const uint8_t D9 = 8;
static const uint8_t D10 = 9;
static const uint8_t D11 = 11;
static const uint8_t D12 = 12;
static const uint8_t D13 = 13;
static const uint8_t D14 = 39;
static const uint8_t D15 = 38;
static const uint8_t D16 = 10;
static const uint8_t D17 = 17;
static const uint8_t D18 = 12;
static const uint8_t D19 = 11;

#endif /* Pins_Arduino_h */
