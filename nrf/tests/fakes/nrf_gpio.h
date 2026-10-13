/* Fake stand-in for Nordic's nrf_gpio.h, used ONLY to host-compile and
 * unit-test this vendor's GPIO adapter under nrf/gpio/. Not a copy of
 * Nordic's header - just enough surface, with real read/write/toggle
 * state behind it, to exercise that adapter's logic on a host machine.
 */
#ifndef HAL_WRAPPER_FAKE_NRF_GPIO_H
#define HAL_WRAPPER_FAKE_NRF_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NRF_GPIO_PIN_NOPULL = 0,
    NRF_GPIO_PIN_PULLDOWN = 1,
    NRF_GPIO_PIN_PULLUP = 3
} nrf_gpio_pin_pull_t;

typedef enum { NRF_GPIO_PIN_DIR_INPUT = 0, NRF_GPIO_PIN_DIR_OUTPUT = 1 } nrf_gpio_pin_dir_t;
typedef enum { NRF_GPIO_PIN_INPUT_CONNECT = 0, NRF_GPIO_PIN_INPUT_DISCONNECT = 1 } nrf_gpio_pin_input_t;
typedef enum { NRF_GPIO_PIN_S0S1 = 0, NRF_GPIO_PIN_S0D1 = 6 } nrf_gpio_pin_drive_t;
typedef enum { NRF_GPIO_PIN_NOSENSE = 0 } nrf_gpio_pin_sense_t;

void nrf_gpio_cfg_input(uint32_t pin, nrf_gpio_pin_pull_t pull);
void nrf_gpio_cfg_output(uint32_t pin);
void nrf_gpio_cfg(uint32_t pin, nrf_gpio_pin_dir_t dir, nrf_gpio_pin_input_t input,
                  nrf_gpio_pin_pull_t pull, nrf_gpio_pin_drive_t drive, nrf_gpio_pin_sense_t sense);
void nrf_gpio_pin_set(uint32_t pin);
void nrf_gpio_pin_clear(uint32_t pin);
uint32_t nrf_gpio_pin_read(uint32_t pin);
void nrf_gpio_pin_toggle(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRF_GPIO_H */
