/* Fake stand-in for Silicon Labs emlib's em_gpio.h, used ONLY to
 * host-compile and unit-test this vendor's classic GPIO adapter under
 * silicon_labs/gpio/hal_port_silabs.c (TARGET_SILABS). Not a copy of
 * Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_EM_GPIO_H
#define HAL_WRAPPER_FAKE_EM_GPIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    gpioPortA = 0, gpioPortB = 1, gpioPortC = 2,
    gpioPortD = 3, gpioPortE = 4, gpioPortF = 5
} GPIO_Port_TypeDef;

typedef enum {
    gpioModeInput, gpioModeInputPull, gpioModePushPull, gpioModeWiredAnd
} GPIO_Mode_TypeDef;

void GPIO_PinModeSet(GPIO_Port_TypeDef port, unsigned int pin, GPIO_Mode_TypeDef mode, unsigned int out);
void GPIO_PinOutSet(GPIO_Port_TypeDef port, unsigned int pin);
void GPIO_PinOutClear(GPIO_Port_TypeDef port, unsigned int pin);
unsigned int GPIO_PinInGet(GPIO_Port_TypeDef port, unsigned int pin);
void GPIO_PinOutToggle(GPIO_Port_TypeDef port, unsigned int pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_GPIO_H */
