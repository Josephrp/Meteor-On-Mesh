#pragma once

#include <cstdint>

#define HW_VARIANT_CUSTOM -1, 256, -1, -1, -1, -1, -1, -1
#define HW_VARIANT_ESP32PP 48, 6, 5, 4, 12, 13, 11, 10
#define HW_VARIANT_MDK_BOARD -1, 4, 11, 10, 12, 13, 5, 6
#define HW_VARIANT_PRFAI -1, 256, 11, 10, 12, 13, 1, 2
// Meshtonic H4M Companion v2 (shared I2C on 5/6 for master+slave to H4M, GPS on UART1 RX=7)
#define HW_VARIANT_MESHTONIC_H4M -1, 7, 5, 6, -1, -1, 5, 6

class PinConfig {
   public:
    PinConfig() {};
    /**
     * @brief Construct a new Pin Config object from a fixed list of pin numbers.
     * This is highly efficient for embedded systems as it involves no templates or dynamic allocation.
     * @param ledRgb Pin for the RGB LED.
     * @param gpsRx Pin for the GPS.
     * @param i2cSda Pin for the I2C SDA (Master).
     * @param i2cScl Pin for the I2C SCL (Master).
     * @param irRx Pin for the IR Receiver.
     * @param irTx Pin for the IR Transmitter.
     * @param i2cSdaSlave Pin for the I2C SDA (Slave).
     * @param i2cSclSlave Pin for the I2C SCL (Slave).
     */
    PinConfig(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;
        // Auto-detect typed profile from well-known presets (enables board-specific behavior)
        if (i2cSda == 5 && i2cScl == 6 && gpsRx == 7 && i2cSdaSlave == 5 && i2cSclSlave == 6) {
            profile = BoardProfile::MESHTONIC_H4M;
            if (radioCount <= 0) radioCount = 1;
        } else if (ledRgb == 48 && gpsRx == 6 && i2cSda == 5) {
            profile = BoardProfile::ESP32PP;
        } else if (i2cSda == 11 && i2cScl == 10 && i2cSdaSlave == 5 && i2cSclSlave == 6) {
            profile = BoardProfile::MDK;
        }
    };

    void setPins(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;
        // Re-detect profile after manual pin changes (supports web UI preset apply)
        if (i2cSdaPin == 5 && i2cSclPin == 6 && gpsRxPin == 7 && i2cSdaSlavePin == 5 && i2cSclSlavePin == 6) {
            profile = BoardProfile::MESHTONIC_H4M;
            if (radioCount <= 0) radioCount = 1;
        }
    };
    bool isPinsOk() { return (i2cSdaSlavePin != -1 && i2cSclSlavePin != -1); }  // the bare minimum

    int32_t LedRgbPin() { return ledRgbPin; }
    int32_t GpsRxPin() { return gpsRxPin; }
    int32_t I2cSdaPin() { return i2cSdaPin; }
    int32_t I2cSclPin() { return i2cSclPin; }
    int32_t IrRxPin() { return irRxPin; }
    int32_t IrTxPin() { return irTxPin; }
    int32_t I2cSdaSlavePin() { return i2cSdaSlavePin; }
    int32_t I2cSclSlavePin() { return i2cSclSlavePin; }

    void saveToNvs();    // save current config to nvs
    void loadFromNvs();  // load config from nvs

    void debugPrint();  // print current config to log

    bool hasGPS() { return (gpsRxPin < 200); }
    bool hasIRrx() { return (irRxPin != -1); }
    bool hasIRtx() { return (irTxPin != -1); }

    // Typed board profile + Meshtonic extras (persisted alongside pins)
    enum class BoardProfile : int32_t {
        CUSTOM = 0,
        ESP32PP = 1,
        MDK = 2,
        PRFAI = 3,
        MESHTONIC_H4M = 10
    };

    BoardProfile getProfile() const { return profile; }
    void setProfile(BoardProfile p) { profile = p; }

    int32_t getRadioCount() const { return radioCount; }
    void setRadioCount(int32_t n) { radioCount = (n < 0 ? 0 : (n > 4 ? 4 : n)); }

    uint32_t getSensorMask() const { return sensorMask; }
    void setSensorMask(uint32_t m) { sensorMask = m; }

    uint8_t getUartMode() const { return uartMode; } // 0=gps, 1=ld2450
    void setUartMode(uint8_t m) { uartMode = (m > 1 ? 0 : m); }

   protected:
    int32_t ledRgbPin = -1;  // -1 = not used. this is the rgb led pin. single pin, that uses ledstrip_controller
    int32_t gpsRxPin = 256;  // 256 = not used this it the uart rx port of the esp, where the gps's tx pin is wired.

    int32_t i2cSdaPin = -1;  // -1 = not used this is the i2c sda pin (master). you'll wire the sda of the sensors here.
    int32_t i2cSclPin = -1;  // -1 = not used this is the i2c scl pin (master). you'll wire the scl of the sensors here.

    int32_t irRxPin = -1;  // -1 = not used this is the ir receiver pin.
    int32_t irTxPin = -1;  // -1 = not used this is the ir transmitter pin.

    int32_t i2cSdaSlavePin = -1;  // -1 = not used this is the i2c sda pin (slave). this will be connected to portapack's i2c master sda pin.
    int32_t i2cSclSlavePin = -1;  // -1 = not used this is the i2c scl pin (slave). this will be connected to portapack's i2c master scl pin.

    // Extended profile state (Meshtonic + future boards)
    BoardProfile profile = BoardProfile::CUSTOM;
    int32_t radioCount = 1;     // 0-4 Wio SX1262 sites enabled
    uint32_t sensorMask = 0;    // bitmask for enabled muxed/onboard sensors
    uint8_t uartMode = 0;       // 0 = GPS (UART1), 1 = LD2450 (mutually exclusive in current HW)
};