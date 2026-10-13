/* Silicon Labs adapter for the GPIO portion of hal_wrapper.h, against
 * the *current* `sl_gpio` driver (Simplicity SDK's "Peripheral HAL"
 * architecture, production-quality as of the 2025 SDK releases,
 * covering Series 2 and Series 3 devices).
 *
 * This is a genuinely different API from classic emlib
 * (hal_port_silabs.c in this same folder, gated by TARGET_SILABS) —
 * not a rename.
 * sl_gpio_t is a {port, pin} struct passed by pointer, not two loose
 * arguments, and every call returns sl_status_t. Verified against
 * platform/service/device_manager/inc/sl_device_gpio.h and
 * platform/driver/gpio/inc/sl_gpio.h in SiliconLabs/simplicity_sdk
 * (sisdk-2025.6) before writing this — see docs/gpio-api-reference.md.
 *
 * Business logic written against hal_wrapper.h doesn't change at all
 * between the two Silabs adapters — only the IDE's target macro
 * (TARGET_SILABS vs TARGET_SILABS_SLGPIO) and the SDK on the include
 * path change. That's the point: migrating emlib -> sl_gpio shouldn't
 * touch a single line of the customer's own code.
 *
 * sl_gpio_set_pin_mode() does NOT enable the GPIO bus clock itself —
 * that's sl_gpio_init()'s job (it calls
 * sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO) and, on parts
 * with GPIO IRQ support, arms the GPIO_ODD/EVEN NVIC lines). Confirmed
 * against platform/driver/gpio/src/sl_gpio.c in SiliconLabs/
 * simplicity_sdk (sisdk-2025.6). sl_gpio_init() is safe to call every
 * time hf_gpio_init() runs — it only touches global clock/interrupt
 * state, never per-pin config, and its own interrupt-enable step is
 * already idempotent (guarded by an is-disabled check), so this
 * doesn't need a "did we already call it" guard of our own.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_GPIO

#include "sl_gpio.h"
#include "sl_status.h"

#define HF_PIN_PORT(p)   ((p) >> 5)
#define HF_PIN_NUM(p)    ((p) & 0x1F)

static sl_gpio_t hf_decode_gpio(hf_gpio_t pin)
{
    sl_gpio_t gpio;
    gpio.port = (uint8_t)HF_PIN_PORT(pin);
    gpio.pin = (uint8_t)HF_PIN_NUM(pin);
    return gpio;
}

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode)
{
    sl_gpio_t gpio = hf_decode_gpio(pin);
    sl_gpio_mode_t sl_mode;
    bool pull_or_initial_state = false;

    (void)sl_gpio_init();

    switch (mode) {
    case HF_GPIO_INPUT:
        sl_mode = SL_GPIO_MODE_INPUT;
        break;
    case HF_GPIO_INPUT_PULLUP:
        /* SL_GPIO_MODE_INPUT_PULL's pull direction is set by the
         * output_value argument (true = pull-up), same as classic
         * emlib's gpioModeInputPull. */
        sl_mode = SL_GPIO_MODE_INPUT_PULL;
        pull_or_initial_state = true;
        break;
    case HF_GPIO_INPUT_PULLDOWN:
        sl_mode = SL_GPIO_MODE_INPUT_PULL;
        pull_or_initial_state = false;
        break;
    case HF_GPIO_OUTPUT_PUSHPULL:
        sl_mode = SL_GPIO_MODE_PUSH_PULL;
        break;
    case HF_GPIO_OUTPUT_OPENDRAIN:
        sl_mode = SL_GPIO_MODE_WIRED_AND;
        break;
    default:
        return false;
    }

    return sl_gpio_set_pin_mode(&gpio, sl_mode, pull_or_initial_state) == SL_STATUS_OK;
}

void hf_gpio_write(hf_gpio_t pin, bool state)
{
    sl_gpio_t gpio = hf_decode_gpio(pin);

    if (state) {
        sl_gpio_set_pin(&gpio);
    } else {
        sl_gpio_clear_pin(&gpio);
    }
}

bool hf_gpio_read(hf_gpio_t pin)
{
    sl_gpio_t gpio = hf_decode_gpio(pin);
    bool value = false;

    /* sl_gpio_get_pin_input reads the actual pad state, same as
     * emlib's GPIO_PinInGet — not sl_gpio_get_pin_output, which would
     * read back the driven register instead. Ignoring the sl_status_t
     * here on purpose: hf_gpio_read's contract is a plain bool, same
     * safe-default-on-failure behavior as the other adapters. */
    sl_gpio_get_pin_input(&gpio, &value);
    return value;
}

void hf_gpio_toggle(hf_gpio_t pin)
{
    sl_gpio_t gpio = hf_decode_gpio(pin);

    sl_gpio_toggle_pin(&gpio);
}

#endif /* HF_ENABLE_GPIO */
#endif /* TARGET_SILABS_SLGPIO */
