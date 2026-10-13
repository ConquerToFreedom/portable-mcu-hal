/* Silicon Labs (emlib) vendor adapter for the GPIO portion of
 * hal_wrapper.h — the CLASSIC/legacy API (still supported, not
 * deprecated, but doesn't cover Series 3 parts). Gated by
 * TARGET_SILABS. For the current sl_gpio driver (Series 2 + Series 3,
 * a genuinely different API, not a rename), see
 * hal_port_silabs_slgpio.c in this same folder behind
 * TARGET_SILABS_SLGPIO instead.
 *
 * Silabs is a port+index vendor, not a bitmask one: GPIO_Port_TypeDef's
 * enum values (gpioPortA=0 .. gpioPortF=5) already match HF_PIN_PORT(p)
 * directly, so unlike STM32 this needs no lookup table — just a cast.
 * See docs/gpio-api-reference.md for the decode pattern.
 *
 * Pull direction on Silabs isn't a separate GPIO_Mode_TypeDef value —
 * gpioModeInputPull uses the `out` argument of GPIO_PinModeSet() to pick
 * pull-up (out=1) vs pull-down (out=0). Open-drain is gpioModeWiredAnd
 * (drives low, floats high) rather than a "PushPullOpenDrain" mode.
 *
 * Unlike STM32 (whose CubeMX-generated MspInit always bundles the port's
 * RCC clock enable into any peripheral config that touches it), classic
 * emlib has no such auto-generated init: GPIO_PinModeSet() is a no-op
 * against an unclocked peripheral until cmuClock_GPIO is turned on, and
 * pre-Series2 parts gate GPIO through cmuClock_HFPER as well. Silicon
 * Labs' own sl_simple_led_init() reference driver does exactly this
 * before its first GPIO_PinModeSet() call — see em_cmu.h — so
 * hf_gpio_init() does the same here, unconditionally and idempotently
 * (CMU_ClockEnable on an already-enabled clock is a harmless no-op),
 * rather than assuming the caller's own board init already did it.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_GPIO

#include "em_gpio.h"
#include "em_cmu.h"

#define HF_PIN_PORT(p)   ((p) >> 5)
#define HF_PIN_NUM(p)    ((p) & 0x1F)

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode)
{
    GPIO_Port_TypeDef port = (GPIO_Port_TypeDef)HF_PIN_PORT(pin);
    unsigned int index = (unsigned int)HF_PIN_NUM(pin);

#if !defined(_SILICON_LABS_32B_SERIES_2)
    CMU_ClockEnable(cmuClock_HFPER, true);
#endif
    CMU_ClockEnable(cmuClock_GPIO, true);

    switch (mode) {
    case HF_GPIO_INPUT:
        GPIO_PinModeSet(port, index, gpioModeInput, 0);
        break;
    case HF_GPIO_INPUT_PULLUP:
        GPIO_PinModeSet(port, index, gpioModeInputPull, 1);
        break;
    case HF_GPIO_INPUT_PULLDOWN:
        GPIO_PinModeSet(port, index, gpioModeInputPull, 0);
        break;
    case HF_GPIO_OUTPUT_PUSHPULL:
        GPIO_PinModeSet(port, index, gpioModePushPull, 0);
        break;
    case HF_GPIO_OUTPUT_OPENDRAIN:
        GPIO_PinModeSet(port, index, gpioModeWiredAnd, 0);
        break;
    default:
        return false;
    }
    return true;
}

void hf_gpio_write(hf_gpio_t pin, bool state)
{
    GPIO_Port_TypeDef port = (GPIO_Port_TypeDef)HF_PIN_PORT(pin);
    unsigned int index = (unsigned int)HF_PIN_NUM(pin);

    if (state) {
        GPIO_PinOutSet(port, index);
    } else {
        GPIO_PinOutClear(port, index);
    }
}

bool hf_gpio_read(hf_gpio_t pin)
{
    GPIO_Port_TypeDef port = (GPIO_Port_TypeDef)HF_PIN_PORT(pin);
    unsigned int index = (unsigned int)HF_PIN_NUM(pin);

    /* GPIO_PinInGet reads the DIN register (actual pad state) — the
     * Silabs equivalent of STM32's HAL_GPIO_ReadPin / nRF's
     * nrf_gpio_pin_read. GPIO_PinOutGet reads DOUT (what we last drove)
     * instead, which is the wrong function for reading an input pin. */
    return GPIO_PinInGet(port, index) != 0;
}

void hf_gpio_toggle(hf_gpio_t pin)
{
    GPIO_Port_TypeDef port = (GPIO_Port_TypeDef)HF_PIN_PORT(pin);
    unsigned int index = (unsigned int)HF_PIN_NUM(pin);

    GPIO_PinOutToggle(port, index);
}

#endif /* HF_ENABLE_GPIO */
#endif /* TARGET_SILABS */
