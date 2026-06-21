#pragma once

#include <stdint.h>

void meshtonicRadioGpioInit();
void meshtonicRadioGpioWrite(uint32_t pin, uint32_t value);
uint32_t meshtonicRadioGpioRead(uint32_t pin);

#if defined(MESHTONIC_H4M_MCP_RADIO)
void meshtonicMcpDigitalWrite(uint32_t pin, uint32_t value);
uint32_t meshtonicMcpDigitalRead(uint32_t pin);
void meshtonicMcpEnableRadioInterrupt(void (*callback)());
void meshtonicMcpDisableRadioInterrupt();
#endif
