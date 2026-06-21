#pragma once

#include <stdint.h>

#define MESHTONIC_MCP23017_REG_IODIRA 0x00
#define MESHTONIC_MCP23017_REG_IODIRB 0x01
#define MESHTONIC_MCP23017_REG_GPPUA 0x0C
#define MESHTONIC_MCP23017_REG_GPPUB 0x0D
#define MESHTONIC_MCP23017_REG_INTCONA 0x08
#define MESHTONIC_MCP23017_REG_DEFVALA 0x06
#define MESHTONIC_MCP23017_REG_GPINTENA 0x04
#define MESHTONIC_MCP23017_REG_GPIOA 0x12
#define MESHTONIC_MCP23017_REG_OLATA 0x14
#define MESHTONIC_MCP23017_REG_IOCON 0x0A

void meshtonicMcp23017Init();
bool meshtonicMcp23017WriteRegister(uint8_t reg, uint8_t value);
bool meshtonicMcp23017ReadRegister(uint8_t reg, uint8_t *value);
void meshtonicMcp23017WritePin(uint8_t pin, bool level);
bool meshtonicMcp23017ReadPin(uint8_t pin);
