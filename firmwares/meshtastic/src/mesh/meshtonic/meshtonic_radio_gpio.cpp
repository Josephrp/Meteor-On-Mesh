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

    // v2: WIO1 CS + DIO1 are native XIAO GPIOs; only BUSY lives on the MCP23017.
    pinMode(MESHTONIC_WIO1_CS_GPIO, OUTPUT);
    digitalWrite(MESHTONIC_WIO1_CS_GPIO, HIGH); // CS idle high (deasserted)
    pinMode(MESHTONIC_WIO1_DIO1_GPIO, INPUT);
}

void meshtonicRadioGpioWrite(uint32_t pin, uint32_t value)
{
    switch (pin) {
    case MESHTONIC_VPIN_CS:
        digitalWrite(MESHTONIC_WIO1_CS_GPIO, value != 0 ? HIGH : LOW); // native CS
        break;
    default:
        break;
    }
}

uint32_t meshtonicRadioGpioRead(uint32_t pin)
{
    switch (pin) {
    case MESHTONIC_VPIN_DIO1:
        return digitalRead(MESHTONIC_WIO1_DIO1_GPIO) ? 1 : 0; // native DIO1
    case MESHTONIC_VPIN_BUSY:
        return meshtonicMcp23017ReadPin(MESHTONIC_MCP_WIO1_BUSY) ? 1 : 0; // MCP GPA0
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
    // v2: WIO1 DIO1 is a native GPIO, so attach the radio IRQ directly to it
    // (active-high → RISING) rather than to the MCP INTA line.
    meshtonicRadioIsrCallback = callback;
    attachInterrupt(digitalPinToInterrupt(MESHTONIC_WIO1_DIO1_GPIO), meshtonicMcpIntaIsr, RISING);
}

void meshtonicMcpDisableRadioInterrupt()
{
    detachInterrupt(digitalPinToInterrupt(MESHTONIC_WIO1_DIO1_GPIO));
    meshtonicRadioIsrCallback = nullptr;
}
#endif
