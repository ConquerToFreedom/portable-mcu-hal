/* Fake stand-in for Silicon Labs' current sl_gpio.h (Simplicity SDK's
 * "Peripheral HAL"), used ONLY to host-compile and unit-test this
 * vendor's current GPIO adapter under
 * silicon_labs/gpio/hal_port_silabs_slgpio.c (TARGET_SILABS_SLGPIO).
 * Not a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_SL_GPIO_H
#define HAL_WRAPPER_FAKE_SL_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t port;
    uint8_t pin;
} sl_gpio_t;

#define SL_GPIO_PORT_A 0
#define SL_GPIO_PORT_B 1
#define SL_GPIO_PORT_C 2
#define SL_GPIO_PORT_D 3

typedef enum {
    SL_GPIO_MODE_DISABLED,
    SL_GPIO_MODE_INPUT,
    SL_GPIO_MODE_INPUT_PULL,
    SL_GPIO_MODE_PUSH_PULL,
    SL_GPIO_MODE_WIRED_AND
} sl_gpio_mode_t;

sl_status_t sl_gpio_init(void);
sl_status_t sl_gpio_set_pin_mode(sl_gpio_t *gpio, sl_gpio_mode_t mode, bool value);
void sl_gpio_set_pin(sl_gpio_t *gpio);
void sl_gpio_clear_pin(sl_gpio_t *gpio);
sl_status_t sl_gpio_get_pin_input(sl_gpio_t *gpio, bool *value);
void sl_gpio_toggle_pin(sl_gpio_t *gpio);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SL_GPIO_H */
