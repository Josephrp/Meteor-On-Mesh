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

    // v2: WIO1 CS + DIO1 are native XIAO GPIOs; only WIO1 BUSY (GPA0) is on the MCP.
    const uint8_t iodirA = (1U << MESHTONIC_MCP_WIO1_BUSY); // BUSY = input

    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_IOCON, 0x02);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_IODIRA, iodirA);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_GPPUA, 0x00);
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_OLATA, 0x00);
    // No MCP interrupt source: WIO1 DIO1 is native (its IRQ attaches directly in
    // the radio GPIO bridge), so GPINTENA stays disabled.
    meshtonicMcp23017WriteRegister(MESHTONIC_MCP23017_REG_GPINTENA, 0x00);

    mcpReady = true;
    LOG_INFO("MCP23017 init OK (v2: WIO1 BUSY=GPA0; CS=GPIO%d DIO1=GPIO%d native)",
             MESHTONIC_WIO1_CS_GPIO, MESHTONIC_WIO1_DIO1_GPIO);
}
