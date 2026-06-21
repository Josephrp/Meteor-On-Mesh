#pragma once

#include <stdint.h>

#include "detect/ScanI2C.h"

#include <map>

void meshtonicTca9548aInit();
void meshtonicTca9548aSelectChannel(uint8_t channel);
void meshtonicTca9548aSelectForAddress(uint8_t address);
void meshtonicTca9548aRegisterMuxedDevices(
    ScanI2C::I2CPort port, std::map<ScanI2C::DeviceType, ScanI2C::DeviceAddress> &deviceAddresses,
    std::map<ScanI2C::DeviceAddress, ScanI2C::DeviceType> &foundDevices);
