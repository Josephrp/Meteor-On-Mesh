/*
 * SX1262 driver implementation for Meshtonic (onboard Wio modules + antennas).
 * SPI via shared bus + MCP slot selection for CS.
 */

#include "lora_radio.h"
#include "sx_manager.hpp"
#include "meshtonic_board.h"
#include "spi_bus_manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <algorithm>

static const char *TAG = "lora_radio";
static portMUX_TYPE s_spiMux = portMUX_INITIALIZER_UNLOCKED;
static bool s_monitorMode = false;

void LoraRadio::setMonitorMode(bool enabled) { s_monitorMode = enabled; }

// SX126x commands (common subset)
#define SX126X_CMD_SET_SLEEP            0x84
#define SX126X_CMD_SET_STANDBY          0x80
#define SX126X_CMD_SET_PACKET_TYPE      0x8A
#define SX126X_CMD_SET_RF_FREQUENCY     0x86
#define SX126X_CMD_SET_MODULATION_PARAMS 0x8B
#define SX126X_CMD_SET_PACKET_PARAMS    0x8C
#define SX126X_CMD_SET_DIO_IRQ_PARAMS   0x8D
#define SX126X_CMD_SET_RX               0x82
#define SX126X_CMD_SET_CAD              0xC5
#define SX126X_CMD_SET_TX               0x83
#define SX126X_CMD_GET_STATUS           0xC0
#define SX126X_CMD_GET_PACKET_STATUS    0x14
#define SX126X_CMD_READ_BUFFER          0x1E
#define SX126X_CMD_WRITE_BUFFER         0x0E
#define SX126X_CMD_WRITE_REGISTER       0x0D
#define SX126X_CMD_READ_REGISTER        0x1D
#define SX126X_CMD_SET_DIO2_AS_RF_SW    0x9D
#define SX126X_CMD_SET_DIO3_AS_TCXO     0x97
#define SX126X_CMD_GET_IRQ_STATUS       0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS     0x02
#define SX126X_CMD_GET_RX_BUFFER_STATUS 0x13

// Packet type
#define PKT_TYPE_LORA 0x01

extern SXRadioManager sxManager;

LoraRadio::LoraRadio(int s) : slot(s) {}

esp_err_t LoraRadio::init(spi_host_device_t host, int mosi, int miso, int sclk, int freq_hz) {
    // Ensure shared bus (GPIO38/39/40) is up exactly once (display or other users may pre-init).
    esp_err_t err = spi_bus_manager_init(host, mosi, miso, sclk, 4096);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_manager_init failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 0;
    devcfg.address_bits = 0;
    devcfg.dummy_bits = 0;
    devcfg.mode = 0;                 // SPI mode 0 for SX126x
    devcfg.clock_speed_hz = freq_hz;
    devcfg.spics_io_num = -1;        // we control CS via MCP
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    devcfg.queue_size = 4;

    err = spi_bus_add_device(host, &devcfg, &spi_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed");
        return err;
    }
    return ESP_OK;
}

void LoraRadio::deinit() {
    if (spi_dev) {
        spi_bus_remove_device(spi_dev);
        spi_dev = nullptr;
    }
}

bool LoraRadio::select() {
    if (!sxManager.selectSlot(slot)) return false;
    selected = true;
    return true;
}

esp_err_t LoraRadio::writeCommand(uint8_t cmd, const uint8_t* data, size_t len) {
    portENTER_CRITICAL(&s_spiMux);
    if (!select() || !spi_dev) { portEXIT_CRITICAL(&s_spiMux); return ESP_ERR_INVALID_STATE; }

    spi_transaction_t t = {};
    t.length = 8 + len * 8;
    uint8_t tx[32] = {cmd};
    if (data && len) memcpy(tx+1, data, len);
    t.tx_buffer = tx;
    t.rx_buffer = nullptr;

    esp_err_t err = spi_device_transmit(spi_dev, &t);
    sxManager.spiDeselectAll();  // deassert after
    selected = false;
    portEXIT_CRITICAL(&s_spiMux);
    return err;
}

esp_err_t LoraRadio::readCommand(uint8_t cmd, uint8_t* data, size_t len) {
    portENTER_CRITICAL(&s_spiMux);
    if (!select() || !spi_dev) { portEXIT_CRITICAL(&s_spiMux); return ESP_ERR_INVALID_STATE; }

    spi_transaction_t t = {};
    t.length = 8 + (1 + len) * 8;  // status + data
    uint8_t tx[32] = {cmd, 0};
    uint8_t rx[32] = {};
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(spi_dev, &t);
    if (err == ESP_OK && data && len) {
        memcpy(data, rx + 2, len);  // skip status + dummy often
    }
    sxManager.spiDeselectAll();
    selected = false;
    portEXIT_CRITICAL(&s_spiMux);
    return err;
}

esp_err_t LoraRadio::writeRegister(uint16_t addr, uint8_t val) {
    uint8_t buf[3] = { (uint8_t)(addr >> 8), (uint8_t)addr, val };
    return writeCommand(SX126X_CMD_WRITE_REGISTER, buf, 3);
}

esp_err_t LoraRadio::readRegister(uint16_t addr, uint8_t* val) {
    portENTER_CRITICAL(&s_spiMux);
    if (!select() || !spi_dev || !val) {
        portEXIT_CRITICAL(&s_spiMux);
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t t = {};
    t.length = 8 * (1 + 2 + 1 + 1); // cmd + addr + NOP + status/data
    uint8_t tx[5] = {
        SX126X_CMD_READ_REGISTER,
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
        0,
        0
    };
    uint8_t rx[5] = {};
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(spi_dev, &t);
    if (err == ESP_OK) {
        *val = rx[4];
    }
    sxManager.spiDeselectAll();
    selected = false;
    portEXIT_CRITICAL(&s_spiMux);
    return err;
}

esp_err_t LoraRadio::reset() {
    // LORA_RST is a single shared net (MCP GPB4) across all 4 radios, so we do NOT
    // pulse it per-slot during arming (that would reset already-configured radios).
    // Soft standby is the per-radio entry point; a global hardware reset is a
    // separate manager-level action when needed.
    ESP_LOGD(TAG, "reset: shared LORA_RST (slot %d), using soft standby", slot);
    return setStandby();
}

esp_err_t LoraRadio::setStandby() {
    uint8_t p = 0; // RC
    return writeCommand(SX126X_CMD_SET_STANDBY, &p, 1);
}

esp_err_t LoraRadio::setPacketTypeLoRa() {
    uint8_t t = PKT_TYPE_LORA;
    return writeCommand(SX126X_CMD_SET_PACKET_TYPE, &t, 1);
}

esp_err_t LoraRadio::setFrequency(uint32_t freq_hz) {
    uint32_t rf = (uint32_t)((uint64_t)freq_hz * (1ULL << 25) / 32000000ULL);
    uint8_t buf[4] = {
        (uint8_t)(rf >> 24), (uint8_t)(rf >> 16), (uint8_t)(rf >> 8), (uint8_t)rf
    };
    return writeCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4);
}

esp_err_t LoraRadio::setModulationParams(uint8_t sf, uint32_t bw_hz, uint8_t cr, uint8_t ldro) {
    // BW codes per SX126x datasheet Table 13-48 (LoRa)
    uint8_t bw_code = 0x04; // 125 kHz default
    if      (bw_hz >= 500000) bw_code = 0x06;
    else if (bw_hz >= 250000) bw_code = 0x05;
    else if (bw_hz >= 125000) bw_code = 0x04;
    else if (bw_hz >=  62500) bw_code = 0x03;
    else if (bw_hz >=  41670) bw_code = 0x02; // ~41.7
    else if (bw_hz >=  31250) bw_code = 0x01; // 31.25? adjust
    else if (bw_hz >=  20830) bw_code = 0x08; // ~20.8
    else bw_code = 0x00; // 7.8 kHz lowest
    // For uncommon low BW use 0x00=7.8 ...; caller typically uses 125/250/500
    // CR: datasheet uses 0x01=4/5 ... 0x04=4/8 ; our cr param 1..4 maps directly
    uint8_t cr_code = (cr >= 1 && cr <= 4) ? cr : 1;
    uint8_t buf[4] = {sf, bw_code, cr_code, static_cast<uint8_t>(ldro ? 1 : 0)};
    return writeCommand(SX126X_CMD_SET_MODULATION_PARAMS, buf, 4);
}

esp_err_t LoraRadio::setPacketParams(uint16_t preamble, bool explicitHeader, uint8_t payloadLen, bool crcOn, bool invertIQ) {
    uint8_t buf[6] = {
        (uint8_t)(preamble >> 8), (uint8_t)preamble,
        static_cast<uint8_t>(explicitHeader ? 0 : 1),
        payloadLen,
        static_cast<uint8_t>(crcOn ? 1 : 0),
        static_cast<uint8_t>(invertIQ ? 1 : 0)
    };
    return writeCommand(SX126X_CMD_SET_PACKET_PARAMS, buf, 6);
}

esp_err_t LoraRadio::setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask) {
    uint8_t buf[8] = {
        (uint8_t)(irqMask >> 8), (uint8_t)irqMask,
        (uint8_t)(dio1Mask >> 8), (uint8_t)dio1Mask,
        0,0, 0,0   // dio2, dio3
    };
    return writeCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, buf, 8);
}

esp_err_t LoraRadio::setRx(uint32_t timeout_symbols) {
    // timeout in symbols, 0 = continuous
    uint8_t buf[3] = { (uint8_t)(timeout_symbols>>16), (uint8_t)(timeout_symbols>>8), (uint8_t)timeout_symbols };
    return writeCommand(SX126X_CMD_SET_RX, buf, 3);
}

esp_err_t LoraRadio::setCad() {
    return writeCommand(SX126X_CMD_SET_CAD, nullptr, 0);
}

esp_err_t LoraRadio::setTx(uint32_t timeout_symbols) {
    if (s_monitorMode) {
        ESP_LOGW(TAG, "setTx blocked: LoRa decoder monitor mode (no TX)");
        return ESP_ERR_NOT_ALLOWED;
    }
    uint8_t buf[3] = { (uint8_t)(timeout_symbols>>16), (uint8_t)(timeout_symbols>>8), (uint8_t)timeout_symbols };
    return writeCommand(SX126X_CMD_SET_TX, buf, 3);
}

esp_err_t LoraRadio::setSyncWord(uint16_t syncWord) {
    // LoRa private sync word lives in registers 0x0740/0x0741
    esp_err_t e1 = writeRegister(0x0740, (uint8_t)(syncWord >> 8));
    esp_err_t e2 = writeRegister(0x0741, (uint8_t)(syncWord & 0xFF));
    return (e1 == ESP_OK && e2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t LoraRadio::setDio2AsRfSwitch(bool enable) {
    uint8_t v = enable ? 1 : 0;
    return writeCommand(SX126X_CMD_SET_DIO2_AS_RF_SW, &v, 1);
}

esp_err_t LoraRadio::setDio3TcxoCtrl(float voltageVolts) {
    // voltage code: 0=1.6V, 1=1.7, 2=1.8, 3=2.2, 4=2.4, 5=2.7, 6=3.0, 7=3.3
    uint8_t code = 0;
    if (voltageVolts >= 3.3f) code = 7;
    else if (voltageVolts >= 3.0f) code = 6;
    else if (voltageVolts >= 2.7f) code = 5;
    else if (voltageVolts >= 2.4f) code = 4;
    else if (voltageVolts >= 2.2f) code = 3;
    else if (voltageVolts >= 1.8f) code = 2;
    else if (voltageVolts >= 1.7f) code = 1;
    else code = 0;
    uint8_t buf[4] = { code, 0x00, 0x00, 0x00 }; // timeout 0 (use default)
    return writeCommand(SX126X_CMD_SET_DIO3_AS_TCXO, buf, 4);
}

esp_err_t LoraRadio::setRxBoost(bool enable) {
    // DS 13.4.1 / RadioLib: reg 0x08AC controls LNA boost for LoRa RX/CAD
    return writeRegister(0x08AC, enable ? 0x96 : 0x94);
}

esp_err_t LoraRadio::setCurrentLimit(uint8_t ma) {
    // OCP configuration; rough mapping, typical 60-140. See DS 13.1.8 or RadioLib trim.
    // For simplicity write the raw (many impls scale); here pass a register-friendly value or ma/2.5 ish.
    uint8_t v = (ma > 140) ? 0x38 : (ma < 60 ? 0x18 : (uint8_t)((ma - 45) / 2.5f));
    return writeRegister(0x08E7, v); // OCP trim reg example
}

esp_err_t LoraRadio::getDeviceErrors(uint16_t* err) {
    // Device errors often via GetStatus or dedicated; use a read reg or status extension.
    uint8_t st = getStatus();
    if (err) *err = st; // minimal
    return ESP_OK;
}

uint8_t LoraRadio::getStatus() {
    uint8_t st = 0;
    readCommand(SX126X_CMD_GET_STATUS, &st, 1);
    return st;
}

esp_err_t LoraRadio::getIrqStatus(uint16_t* irq) {
    uint8_t buf[2] = {0};
    esp_err_t err = readCommand(SX126X_CMD_GET_IRQ_STATUS, buf, 2);
    if (err == ESP_OK && irq) *irq = ((uint16_t)buf[0] << 8) | buf[1];
    return err;
}

esp_err_t LoraRadio::clearIrqStatus(uint16_t mask) {
    uint8_t buf[2] = { (uint8_t)(mask >> 8), (uint8_t)mask };
    return writeCommand(SX126X_CMD_CLEAR_IRQ_STATUS, buf, 2);
}

esp_err_t LoraRadio::getRxBufferStatus(uint8_t* payloadLen, uint8_t* rxStartBufferPointer) {
    uint8_t buf[2] = {};
    esp_err_t err = readCommand(SX126X_CMD_GET_RX_BUFFER_STATUS, buf, 2);
    if (err == ESP_OK) {
        if (payloadLen) *payloadLen = buf[0];
        if (rxStartBufferPointer) *rxStartBufferPointer = buf[1];
    }
    return err;
}

bool LoraRadio::isBusy() {
    return readBusy();
}

bool LoraRadio::readDio1() {
    // WIO1 DIO1 is a native ESP32-S3 GPIO; WIO2..4 DIO1 are on the MCP23017.
    if (sxManager.isDio1Native(slot)) {
        return gpio_get_level((gpio_num_t)sxManager.getDio1Gpio(slot)) != 0;
    }
    if (!g_mcp_ready) return false;
    bool v = false;
    int p = sxManager.getDio1McpPin(slot);
    if (p != 0xFF) mcp23017_read_pin(&g_mcp, static_cast<unsigned char>(p), &v);
    return v;
}

bool LoraRadio::readBusy() {
    if (!g_mcp_ready) return false;
    bool v = false;
    int p = sxManager.getBusyMcpPin(slot);
    if (p != 0xFF) mcp23017_read_pin(&g_mcp, static_cast<unsigned char>(p), &v);
    return v;
}

esp_err_t LoraRadio::getPacketStatus(int8_t* rssiPkt, int8_t* snrPkt, int8_t* signalRssiPkt) {
    // Returns 5 bytes per DS: status, rssiPkt, snrPkt, signalRssiPkt, ...
    uint8_t buf[5] = {};
    esp_err_t err = readCommand(SX126X_CMD_GET_PACKET_STATUS, buf, 5);
    if (err == ESP_OK) {
        // buf[1] = rssiPkt (-dBm = value/2), buf[2]=snr/4 (signed), buf[3]=signal rssi
        if (rssiPkt) *rssiPkt = -((int8_t)buf[1] / 2);
        if (snrPkt)   *snrPkt   =  (int8_t)buf[2] / 4;
        if (signalRssiPkt) *signalRssiPkt = -((int8_t)buf[3] / 2);
    }
    return err;
}

esp_err_t LoraRadio::readBuffer(uint8_t* buf, uint8_t* len, uint8_t maxLen) {
    // SX126x ReadBuffer: offset 0 is safe for explicit-header single-packet RX (startPtr ignored).
    // SPI is serialized via s_spiMux; MCP I2C (DIO1/BUSY) is not — only call from decoder task.
    portENTER_CRITICAL(&s_spiMux);
    if (!select() || !spi_dev) { portEXIT_CRITICAL(&s_spiMux); return ESP_ERR_INVALID_STATE; }

    uint8_t tx[3] = { SX126X_CMD_READ_BUFFER, 0, 0 };
    uint8_t rx[3 + 255] = {};
    spi_transaction_t t = {};
    t.length = 8 * (2 + maxLen);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(spi_dev, &t);
    sxManager.spiDeselectAll();
    selected = false;
    portEXIT_CRITICAL(&s_spiMux);

    if (err != ESP_OK) return err;

    uint8_t l = maxLen;
    if (buf) {
        const uint8_t start = 2;
        for (uint8_t i = 0; i < l && (start + i) < sizeof(rx); ++i) {
            buf[i] = rx[start + i];
        }
    }
    if (len) *len = l;
    return ESP_OK;
}

esp_err_t LoraRadio::configureFor(const LoraConfig& cfg) {
    // Full bring-up per SX126x + Meshtonic H4M variant (TCXO 1.8V, DIO2 as RF switch, sync 0x1424)
    esp_err_t e = ESP_OK;
    if ((e = reset()) != ESP_OK) return e;
    if ((e = setStandby()) != ESP_OK) return e;
    if ((e = setPacketTypeLoRa()) != ESP_OK) return e;
    if ((e = setFrequency(cfg.freq_hz)) != ESP_OK) return e;
    if ((e = setModulationParams(cfg.sf, cfg.bw_hz, cfg.cr, cfg.ldro)) != ESP_OK) return e;
    if ((e = setPacketParams(cfg.preamble_syms, cfg.explicit_header, 0xFF /*max*/, cfg.crc_on)) != ESP_OK) return e;
    if ((e = setSyncWord(0x1424)) != ESP_OK) return e; // Meshtastic / private
    if ((e = setDio2AsRfSwitch(true)) != ESP_OK) return e;
    if ((e = setDio3TcxoCtrl(1.8f)) != ESP_OK) return e;
    if ((e = setRxBoost(true)) != ESP_OK) return e; // per DS / RadioLib for better CAD/RX on Meshtonic
    // IRQ mask: enable common events on DIO1
    if ((e = setDioIrqParams(0x03FF, 0x03FF)) != ESP_OK) return e;
    return ESP_OK;
}

esp_err_t LoraRadio::startCad() {
    return setCad();
}

esp_err_t LoraRadio::startRxContinuous() {
    return setRx(0);
}

bool LoraRadio::checkRxDone(uint8_t* outLen, uint8_t maxLen, uint8_t* outBuf, int8_t* outRssi, int8_t* outSnr) {
    // maxLen must stay <= 255 (SX1262 buffer); caller uses 255 cap proven in production.
    uint16_t irq = 0;
    if (getIrqStatus(&irq) != ESP_OK) return false;
    const uint16_t RX_DONE = (1u << 1);
    if ((irq & RX_DONE) == 0) return false;
    clearIrqStatus(RX_DONE | (1u << 0) | (1u << 2) | (1u << 3));

    uint8_t actualLen = 0;
    uint8_t startPtr = 0; // ignored: explicit-header single-packet case uses offset 0
    getRxBufferStatus(&actualLen, &startPtr);
    (void)startPtr;

    int8_t r = -127, sn = 0, sig = 0;
    getPacketStatus(&r, &sn, &sig);
    if (outRssi) *outRssi = r;
    if (outSnr) *outSnr = sn;

    uint8_t l = actualLen;
    if (l == 0 || l > maxLen) l = maxLen;

    uint8_t tmpLen = 0;
    if (readBuffer(outBuf, &tmpLen, l) == ESP_OK && outLen) {
        *outLen = (tmpLen > 0 && tmpLen <= l) ? tmpLen : l;
    } else if (outLen) {
        *outLen = l;
    }
    return true;
}

void LoraRadio::handleDio1Irq() {
    // Lightweight: IRQ details drained in checkRxDone from the app task (not ISR SPI).
}
