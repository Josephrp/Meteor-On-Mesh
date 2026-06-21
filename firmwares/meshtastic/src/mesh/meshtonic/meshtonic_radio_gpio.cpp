#include "meshtonic_radio_gpio.h"

#include "meshtonic_mcp23017.h"
#include "variant.h"

#include <Arduino.h>

#if defined(MESHTONIC_H4M_MCP_RADIO)
static void (*meshtonicRadioIsrCallback)() = nullptr;

static void meshtonicMcpIntaIsr()
{
    if (meshtonicRadioIsrCallback) {
        meshtonicRadioIsrCallback();
    }
}
#endif

void meshtonicRadioGpioInit()
{
    meshtonicMcp23017Init();
}

void meshtonicRadioGpioWrite(uint32_t pin, uint32_t value)
{
    switch (pin) {
    case MESHTONIC_VPIN_CS:
        meshtonicMcp23017WritePin(MESHTONIC_MCP_GPA3_CS, value != 0);
        break;
    default:
        break;
    }
}

uint32_t meshtonicRadioGpioRead(uint32_t pin)
{
    switch (pin) {
    case MESHTONIC_VPIN_DIO1:
        return meshtonicMcp23017ReadPin(MESHTONIC_MCP_GPA5_DIO1) ? 1 : 0;
    case MESHTONIC_VPIN_BUSY:
        return meshtonicMcp23017ReadPin(MESHTONIC_MCP_GPA7_BUSY) ? 1 : 0;
    default:
        return 0;
    }
}

#if defined(MESHTONIC_H4M_MCP_RADIO)
void meshtonicMcpDigitalWrite(uint32_t pin, uint32_t value)
{
    meshtonicRadioGpioWrite(pin, value);
}

uint32_t meshtonicMcpDigitalRead(uint32_t pin)
{
    return meshtonicRadioGpioRead(pin);
}

void meshtonicMcpEnableRadioInterrupt(void (*callback)())
{
    meshtonicRadioIsrCallback = callback;
    attachInterrupt(digitalPinToInterrupt(MESHTONIC_MCP_INTA_PIN), meshtonicMcpIntaIsr, FALLING);
}

void meshtonicMcpDisableRadioInterrupt()
{
    detachInterrupt(digitalPinToInterrupt(MESHTONIC_MCP_INTA_PIN));
    meshtonicRadioIsrCallback = nullptr;
}
#endif
