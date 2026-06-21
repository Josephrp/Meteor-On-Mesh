#include "meshtonic_board.h"

#include "configuration.h"

#ifdef MESHTONIC_H4M

#include "meshtonic_radio_gpio.h"
#include "meshtonic_tca9548a.h"

void meshtonicBoardSetup()
{
    meshtonicRadioGpioInit();
    meshtonicTca9548aInit();
}

#endif
