#include <nvs.h>
#include <nvs_flash.h>
#include "pinconfig.h"
#include "esp_log.h"

void PinConfig::saveToNvs() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("pin_config", NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        nvs_set_i32(nvsHandle, "led_rgb_pin", ledRgbPin);
        nvs_set_i32(nvsHandle, "gps_rx_pin", gpsRxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_pin", i2cSdaPin);
        nvs_set_i32(nvsHandle, "i2c_scl_pin", i2cSclPin);
        nvs_set_i32(nvsHandle, "ir_rx_pin", irRxPin);
        nvs_set_i32(nvsHandle, "ir_tx_pin", irTxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_sl", i2cSdaSlavePin);
        nvs_set_i32(nvsHandle, "i2c_scl_sl", i2cSclSlavePin);

        // Extended Meshtonic / board profile fields
        nvs_set_i32(nvsHandle, "board_profile", static_cast<int32_t>(profile));
        nvs_set_i32(nvsHandle, "mt_radio_cnt", radioCount);
        nvs_set_i32(nvsHandle, "mt_sensors", static_cast<int32_t>(sensorMask));
        nvs_set_u8(nvsHandle, "mt_uart_mode", uartMode);

        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    } else {
        ESP_LOGE("PinConfig", "Failed to open NVS for writing: %s", esp_err_to_name(err));
    }
}

void PinConfig::loadFromNvs() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("pin_config", NVS_READONLY, &nvsHandle);
    if (err == ESP_OK) {
        nvs_get_i32(nvsHandle, "led_rgb_pin", &ledRgbPin);
        nvs_get_i32(nvsHandle, "gps_rx_pin", &gpsRxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_pin", &i2cSdaPin);
        nvs_get_i32(nvsHandle, "i2c_scl_pin", &i2cSclPin);
        nvs_get_i32(nvsHandle, "ir_rx_pin", &irRxPin);
        nvs_get_i32(nvsHandle, "ir_tx_pin", &irTxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_sl", &i2cSdaSlavePin);
        nvs_get_i32(nvsHandle, "i2c_scl_sl", &i2cSclSlavePin);

        // Extended fields (defaults remain if missing)
        int32_t prof = static_cast<int32_t>(profile);
        nvs_get_i32(nvsHandle, "board_profile", &prof);
        profile = static_cast<BoardProfile>(prof);

        nvs_get_i32(nvsHandle, "mt_radio_cnt", &radioCount);
        int32_t sm = static_cast<int32_t>(sensorMask);
        nvs_get_i32(nvsHandle, "mt_sensors", &sm);
        sensorMask = static_cast<uint32_t>(sm);
        nvs_get_u8(nvsHandle, "mt_uart_mode", &uartMode);

        nvs_close(nvsHandle);
    } else {
        ESP_LOGE("PinConfig", "Failed to open NVS for reading: %s", esp_err_to_name(err));
        if (isPinsOk()) {
            saveToNvs();  // save current config as default
        }
    }
}

void PinConfig::debugPrint() {
    ESP_LOGI("PinConfig", "Current Pin Configuration:");
    ESP_LOGI("PinConfig", "LED RGB Pin: %ld", ledRgbPin);
    ESP_LOGI("PinConfig", "GPS RX Pin: %ld", gpsRxPin);
    ESP_LOGI("PinConfig", "I2C SDA Pin: %ld", i2cSdaPin);
    ESP_LOGI("PinConfig", "I2C SCL Pin: %ld", i2cSclPin);
    ESP_LOGI("PinConfig", "IR RX Pin: %ld", irRxPin);
    ESP_LOGI("PinConfig", "IR TX Pin: %ld", irTxPin);
    ESP_LOGI("PinConfig", "I2C SDA Slave Pin: %ld", i2cSdaSlavePin);
    ESP_LOGI("PinConfig", "I2C SCL Slave Pin: %ld", i2cSclSlavePin);
    ESP_LOGI("PinConfig", "BoardProfile: %ld  radios:%ld  sensors:0x%08lx  uartMode:%u",
             (long)static_cast<int32_t>(profile), (long)radioCount, (unsigned long)sensorMask, uartMode);
}