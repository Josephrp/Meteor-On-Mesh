// Seeed XIAO ESP32-S3 Plus — Meshtonic H4M v2 Pinout
//
// Verified against pcb/meshtonic_h4m_v2.kicad_pcb netlist and the official
// Espressif XIAO_ESP32S3_Plus D->GPIO mapping (arduino-esp32 pins_arduino.h /
// Seeed wiki). NOTE: on the Plus, Dn != GPIOn above D5 — the mapping is
// irregular (D11=GPIO38 ... D19=GPIO11), so these constants are the REAL
// silicon GPIO numbers, not the D-label numbers.
//
// Strategy: one shared SPI bus (CLK/MOSI/MISO via the ESP32-S3 GPIO matrix) with
// a separate native-GPIO chip-select per Wio-SX1262 shield, so up to 4 radios
// share SPI2/FSPI. Per-radio BUSY/DIO1 that don't fit on native pins are read
// back through an MCP23017 I2C expander.

// UART0 (freed: console runs on USB-Serial-JTAG)
#define UART0_TX    1   // D0
#define UART0_RX    2   // D1

// Wio-SX1262 chip-selects — native ESP32-S3 GPIO (shared SPI bus)
#define CS_WIO1     3   // D2  (NOTE: GPIO3 is a strapping pin / JTAG sel — output, idle high)
#define CS_WIO2     4   // D3
#define CS_WIO3     10  // D16
#define CS_WIO4     13  // D17

// I2C (native — sensors via TCA9548A, MCP23017, AS5600)
#define I2C_SDA     5   // D4
#define I2C_SCL     6   // D5

// Shared SPI bus (Wio-SX1262 x4 + ILI9341 + microSD), routed via GPIO matrix
#define SPI_MOSI    38  // D11
#define SPI_MISO    39  // D12 (JTAG MTCK — fine without a HW debug probe)
#define SPI_CLK     40  // D13 (JTAG MTDO)

// UART1 (GPS NEO-6M / external)
#define UART1_RX    7   // D8
#define UART1_TX    8   // D9

// TFT (ILI9341) control
#define TFT_DC      43  // D6  (UART0 TXD repurposed)
#define TFT_RST     9   // D10
#define TFT_CS      11  // D19
#define MCP_INT     44  // D7  (UART0 RXD repurposed — MCP23017 INTA to ESP)

// Misc native
#define WIO1_DIO1   12  // D18 (slot-0 DIO1 is native; WIO2..4 DIO1 are on the MCP23017)
#define CS_SD       42  // D15 (JTAG MTMS)
#define ADC_LIGHT   41  // D14 (JTAG MTDI — digital only, NOT ADC-capable: read as GPIO or defer)

// MCP23017 @ 0x20 — GPA bank = per-radio status, GPB bank = shared control + LEDs
// GPA0..3 = WIO1..4 BUSY
// GPA4..6 = WIO2..4 DIO1   (WIO1 DIO1 is native GPIO12 above)
// GPB0    = WIO_RF_SW_EN
// GPB1    = CS_GROVE
// GPB4    = LORA_RST (shared across all 4 radios)
// GPB5..7 = LED_WIO1..3
#define MCP23017_ADDR    0x20
#define MCP_BUSY_WIO1    0   // GPA0
#define MCP_BUSY_WIO2    1   // GPA1
#define MCP_BUSY_WIO3    2   // GPA2
#define MCP_BUSY_WIO4    3   // GPA3
#define MCP_DIO1_WIO2    4   // GPA4
#define MCP_DIO1_WIO3    5   // GPA5
#define MCP_DIO1_WIO4    6   // GPA6
#define MCP_RF_SW_EN     8   // GPB0
#define MCP_LORA_RST     12  // GPB4 (shared)
#define MCP_LED_WIO1     13  // GPB5
#define MCP_LED_WIO2     14  // GPB6
#define MCP_LED_WIO3     15  // GPB7
