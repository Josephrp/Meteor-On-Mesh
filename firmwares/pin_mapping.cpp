// Seeed XIAO ESP32-S3 Plus — Meshtonic H4M Pinout

// UART0 (USB Serial)
#define UART0_TX    1   // D0
#define UART0_RX    2   // D1

// Chip Selects (any GPIO)
#define CS_WIO1     3   // D2
#define CS_WIO2     4   // D3

// I2C (Native — Sensors + TCA9548A)
#define I2C_SDA     5   // D4
#define I2C_SCL     6   // D5

// TFT Control
#define TFT_DC      43  // D6
#define TFT_TOUCH_IRQ 44 // D7
#define TFT_RST     9   // D10

// UART1 (GPS / External / Debug)
#define UART1_RX    7   // D8
#define UART1_TX    8   // D9

// SPI Bus (Shared: 2× Wio-SX1262, ILI9341, SD)
#define SPI_MOSI    11  // D11
#define SPI_MISO    12  // D12
#define SPI_CLK     13  // D13

// ADC
#define ADC_LIGHT   39  // D14

// Additional GPIOs (verify exact GPIO numbers in your XIAO symbol)
#define CS_SD       38  // D15 (or D17 — verify)
#define WIO1_BUSY   16  // D16 (verify GPIO)
#define WIO1_DIO1   17  // D18 (verify GPIO)
#define CS_TFT      18  // D19 (verify GPIO)