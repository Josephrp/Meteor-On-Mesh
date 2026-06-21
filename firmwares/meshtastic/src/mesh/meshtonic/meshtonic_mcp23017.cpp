#include "meshtonic_mcp23017.h"

#include "configuration.h"
#include "variant.h"

#include <Wire.h>

#ifndef MESHTONIC_MCP23017_ADDR
#define MESHTONIC_MCP23017_ADDR 0x20
#endif

static bool mcpReady = false;

bool meshtonicMcp23017WriteRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(MESHTONIC_MCP23017_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool meshtonicMcp23017ReadRegister(uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(MESHTONIC_MCP23017_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom((int)MESHTONIC_MCP23017_ADDR, 1) != 1) {
        return false;
    }

    if (!Wire.available()) {
        return false;
    }

    *value = Wire.read();
    return true;
}

static bool meshtonicMcp23017UpdateRegister(uint8_t reg, uint8_t pin, bool setHigh)
{
    uint8_t value = 0;
    if (!meshtonicMcp23017ReadRegister(reg, &value)) {
        return false;
    }

    if (setHigh) {
        value |= (1U << pin);
    } else {
        value &= ~(1U << pin);
    }

    return meshtonicMcp23017WriteRegister(reg, value);
}

void meshtonicMcp23017WritePin(uint8_t pin, bool level)
{
    if (!mcpReady) {
        return;
    }
    meshtonicMcp23017UpdateRegister(MESHTONIC_MCP23017_REG_OLATA, pin, level);
}

bool meshtonicMcp23017ReadPin(uint8_t pin)
{
    uint8_t value = 0;
    if (!mcpReady || !meshtonicMcp23017ReadRegister(MESHTONIC_MCP23017_REG_GPIOA, &value)) {
        return false;
    }
    return (value & (1U << pin)) != 0;
}

void meshtonicMcp23017Init()
{
    Wire.beginTransmission(MESHTONIC_MCP23017_ADDR);
    if (Wire.endTransmission() != 0) {
        LOG_WARN("MCP23017 not found at 0x%02x", MESHTONIC_MCP23017_ADDR);
        mcpReady = false;
        return;
    }

    const uint8_t iodirA = (1U << MESHTONIC_MCP_GPA5_DIO1) | (1U << MESHTONIC_MCP_GPA7_BUSY);
    const uint8_t gppuA = (1U << MESHTONIC_MCP_GPA5_DIO1);
    const uint8_t olatA = (1U << MESHTONIC_MCP_GPA3_CS);

    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_IOCON, 0x02);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_IODIRA, iodirA);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_GPPUA, gppuA);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_OLATA, olatA);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_GPINTENA, (1U << MESHTONIC_MCP_GPA5_DIO1));

    pinMode(MESHTONIC_MCP_INTA_PIN, INPUT_PULLUP);

    mcpReady = true;
    LOG_INFO("MCP23017 init OK (WIO1 CS=GPA3 DIO1=GPA5 BUSY=GPA7 INTA=GPIO%d)", MESHTONIC_MCP_INTA_PIN);
}
