/* TI MSPM0 (driverlib) vendor adapter for the GPIO portion of
 * hal_wrapper.h.
 *
 * Assumes the customer's SysConfig-generated "ti_msp_dl_config.h" is on
 * the include path, which pulls in the correct device header (defining
 * GPIOA/GPIOB) and dl_gpio.h.
 *
 * MSPM0 splits pin configuration across two separate register spaces,
 * unlike STM32/nRF52/Silabs where port+pin fully determines everything:
 *   - GPIO_Regs + bitmask (port level): direction (DL_GPIO_enableOutput/
 *     disableOutput), set/clear/toggle/read. Reachable from hf_gpio_t
 *     alone, same decode pattern as STM32.
 *   - IOMUX PINCM (pin level): pull resistors, open-drain, drive
 *     strength, hysteresis — addressed by a per-pin "PINCM" index that
 *     has NO formulaic relationship to the port+bit encoding in
 *     hf_gpio_t. It's an arbitrary per-chip mapping, normally baked
 *     into SysConfig-generated ti_msp_dl_config.c before main() runs.
 *
 * Consequence: hf_gpio_init() on this vendor can only ever set
 * direction. Pull-up/pull-down/open-drain modes are NOT reachable from
 * the scalar pin identifier at all — see the pull/open-drain
 * asymmetry table in docs/architecture.md. Rather than silently
 * configuring direction and ignoring the requested pull/open-drain
 * (which would leave the customer thinking they got a pull resistor
 * they didn't), this adapter reports failure for those three modes so
 * the gap is loud, not silent. Configure pull resistors and open-drain
 * for a pin in SysConfig's pinmux GUI instead — that's what actually
 * reaches the IOMUX PINCM register on this vendor.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_GPIO

#include "ti_msp_dl_config.h"

#define HF_PIN_PORT(p)   ((p) >> 5)
#define HF_PIN_NUM(p)    ((p) & 0x1F)

#ifdef GPIOA
#define HF_MSPM0_PORT_A GPIOA
#else
#define HF_MSPM0_PORT_A NULL
#endif
#ifdef GPIOB
#define HF_MSPM0_PORT_B GPIOB
#else
#define HF_MSPM0_PORT_B NULL
#endif

/* MSPM0 parts have at most two GPIO ports (A, B) — far fewer than
 * STM32's up to eight — confirmed against the MSPM0G3507's GPIOA/GPIOB
 * struct-pointer convention. */
static GPIO_Regs * const s_port_table[] = {
    HF_MSPM0_PORT_A, HF_MSPM0_PORT_B,
};
#define HF_MSPM0_PORT_COUNT (sizeof(s_port_table) / sizeof(s_port_table[0]))

static GPIO_Regs *hf_decode_port(hf_gpio_t pin)
{
    uint32_t index = HF_PIN_PORT(pin);
    if (index >= HF_MSPM0_PORT_COUNT) {
        return NULL;
    }
    return s_port_table[index];
}

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode)
{
    GPIO_Regs *port = hf_decode_port(pin);
    if (port == NULL) {
        return false;
    }
    uint32_t mask = 1U << HF_PIN_NUM(pin);

    switch (mode) {
    case HF_GPIO_INPUT:
        DL_GPIO_disableOutput(port, mask);
        return true;
    case HF_GPIO_OUTPUT_PUSHPULL:
        DL_GPIO_enableOutput(port, mask);
        return true;
    case HF_GPIO_INPUT_PULLUP:
    case HF_GPIO_INPUT_PULLDOWN:
    case HF_GPIO_OUTPUT_OPENDRAIN:
        /* Genuinely unreachable from hf_gpio_t alone — see file header
         * comment. Fail loudly rather than silently drop the pull/
         * open-drain part of the request. */
        return false;
    default:
        return false;
    }
}

void hf_gpio_write(hf_gpio_t pin, bool state)
{
    GPIO_Regs *port = hf_decode_port(pin);
    if (port == NULL) {
        return;
    }
    uint32_t mask = 1U << HF_PIN_NUM(pin);

    if (state) {
        DL_GPIO_setPins(port, mask);
    } else {
        DL_GPIO_clearPins(port, mask);
    }
}

bool hf_gpio_read(hf_gpio_t pin)
{
    GPIO_Regs *port = hf_decode_port(pin);
    if (port == NULL) {
        return false;
    }
    uint32_t mask = 1U << HF_PIN_NUM(pin);

    return DL_GPIO_readPins(port, mask) != 0;
}

void hf_gpio_toggle(hf_gpio_t pin)
{
    GPIO_Regs *port = hf_decode_port(pin);
    if (port == NULL) {
        return;
    }
    uint32_t mask = 1U << HF_PIN_NUM(pin);

    DL_GPIO_togglePins(port, mask);
}

#endif /* HF_ENABLE_GPIO */
#endif /* TARGET_TI_MSPM0 */
