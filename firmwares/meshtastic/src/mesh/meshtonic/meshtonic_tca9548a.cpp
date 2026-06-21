#include "meshtonic_tca9548a.h"

#include "configuration.h"
#include "variant.h"

#include <Wire.h>
#include <map>

#ifndef MESHTONIC_TCA9548A_ADDR
#define MESHTONIC_TCA9548A_ADDR 0x70
#endif

static bool tcaReady = false;
static uint8_t activeChannel = 0xFF;
static std::map<uint8_t, uint8_t> addressToChannel;

static bool meshtonicTca9548aProbeAddress(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

static uint8_t meshtonicTca9548aReadChipId(uint8_t address, uint8_t reg)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }
    if (Wire.requestFrom((int)address, 1) != 1) {
        return 0;
    }
    if (!Wire.available()) {
        return 0;
    }
    return Wire.read();
}

void meshtonicTca9548aSelectChannel(uint8_t channel)
{
    if (!tcaReady) {
        return;
    }

    if (channel == activeChannel) {
        return;
    }

    const uint8_t mask = channel >= 8 ? 0 : (1U << channel);
    Wire.beginTransmission(MESHTONIC_TCA9548A_ADDR);
    Wire.write(mask);
    if (Wire.endTransmission() == 0) {
        activeChannel = channel;
    }
}

void meshtonicTca9548aSelectForAddress(uint8_t address)
{
    auto it = addressToChannel.find(address);
    if (it == addressToChannel.end()) {
        meshtonicTca9548aSelectChannel(0xFF);
        return;
    }
    meshtonicTca9548aSelectChannel(it->second);
}

void meshtonicTca9548aInit()
{
    Wire.beginTransmission(MESHTONIC_TCA9548A_ADDR);
    tcaReady = Wire.endTransmission() == 0;
    if (tcaReady) {
        meshtonicTca9548aSelectChannel(0xFF);
        LOG_INFO("TCA9548A init OK at 0x%02x", MESHTONIC_TCA9548A_ADDR);
    } else {
        LOG_WARN("TCA9548A not found at 0x%02x", MESHTONIC_TCA9548A_ADDR);
    }
}

static void meshtonicTca9548aRegisterIfFound(
    uint8_t channel, uint8_t address, ScanI2C::DeviceType type, ScanI2C::I2CPort port,
    std::map<ScanI2C::DeviceType, ScanI2C::DeviceAddress> &deviceAddresses,
    std::map<ScanI2C::DeviceAddress, ScanI2C::DeviceType> &foundDevices)
{
    meshtonicTca9548aSelectChannel(channel);
    if (!meshtonicTca9548aProbeAddress(address)) {
        return;
    }

    addressToChannel[address] = channel;
    ScanI2C::DeviceAddress addr(port, address);
    deviceAddresses[type] = addr;
    foundDevices[addr] = type;
    LOG_INFO("Muxed sensor type %d found on TCA ch%u at 0x%02x", (int)type, channel, address);
}

void meshtonicTca9548aRegisterMuxedDevices(
    ScanI2C::I2CPort port, std::map<ScanI2C::DeviceType, ScanI2C::DeviceAddress> &deviceAddresses,
    std::map<ScanI2C::DeviceAddress, ScanI2C::DeviceType> &foundDevices)
{
    if (!tcaReady || port != ScanI2C::I2CPort::WIRE) {
        return;
    }

    LOG_INFO("Scanning TCA9548A mux channels 0-3");

    for (uint8_t channel = 0; channel < 4; channel++) {
        meshtonicTca9548aSelectChannel(channel);

        for (uint8_t addr = 0x10; addr <= 0x13; addr++) {
            if (meshtonicTca9548aProbeAddress(addr)) {
                meshtonicTca9548aRegisterIfFound(channel, addr, ScanI2C::BMM150, port, deviceAddresses, foundDevices);
            }
        }

        for (uint8_t addr : {0x76, 0x77}) {
            if (!meshtonicTca9548aProbeAddress(addr)) {
                continue;
            }
            const uint8_t chipId = meshtonicTca9548aReadChipId(addr, 0xD0);
            if (chipId == 0x58) {
                meshtonicTca9548aRegisterIfFound(channel, addr, ScanI2C::BMP_280, port, deviceAddresses, foundDevices);
            }
        }

        for (uint8_t addr : {0x44, 0x45}) {
            if (meshtonicTca9548aProbeAddress(addr)) {
                meshtonicTca9548aRegisterIfFound(channel, addr, ScanI2C::SHTXX, port, deviceAddresses, foundDevices);
            }
        }

        for (uint8_t addr : {0x68, 0x69}) {
            if (!meshtonicTca9548aProbeAddress(addr)) {
                continue;
            }
            const uint8_t chipId = meshtonicTca9548aReadChipId(addr, 0x00);
            if (chipId == 0xD1) {
                meshtonicTca9548aRegisterIfFound(channel, addr, ScanI2C::BMX160, port, deviceAddresses, foundDevices);
            }
        }
    }

    meshtonicTca9548aSelectChannel(0xFF);
}
