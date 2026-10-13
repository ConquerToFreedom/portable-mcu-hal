/* nRF52 (nrfx) vendor adapter for the GPIO portion of hal_wrapper.h.
 *
 * nrf_gpio.h is not specific to the legacy nRF5 SDK — it lives in the
 * nrfx repo, which is bundled unchanged inside both the legacy nRF5
 * SDK and the current nRF Connect SDK (nRF Connect SDK's hal_nordic is
 * downstream of the same nrfx source). This single adapter covers
 * both generations as long as the customer calls into nrfx directly.
 * There's deliberately no second nRF adapter/target: going through
 * nRF Connect SDK's own Zephyr-based gpio.h driver instead would mean
 * adopting Zephyr's kernel/devicetree/build system, which is exactly
 * the dependency this project is positioned to avoid (see "Why not
 * just use X" in docs/architecture.md) — so that path is out of scope
 * on purpose, not an oversight.
 *
 * hf_gpio_t's (port << 5) | pin encoding was chosen to match Nordic's
 * own NRF_GPIO_PIN_MAP(port, pin) exactly (see docs/architecture.md),
 * so it passes straight through as the nrf_gpio_* pin number — no
 * decode table needed here, unlike the bitmask vendors.
 *
 * Toggle: nRF52's GPIO peripheral has only OUT/OUTSET/OUTCLR, no
 * dedicated atomic toggle register (confirmed against Nordic's own
 * nrf_gpio_pin_toggle(), which does a read-modify-write of OUT into
 * OUTSET/OUTCLR — see the toggle asymmetry table in
 * docs/architecture.md). That function already does the RMW correctly,
 * so this adapter calls it directly instead of reimplementing it.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_GPIO

#include "nrf_gpio.h"

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode)
{
    switch (mode) {
    case HF_GPIO_INPUT:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
        break;
    case HF_GPIO_INPUT_PULLUP:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
        break;
    case HF_GPIO_INPUT_PULLDOWN:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
        break;
    case HF_GPIO_OUTPUT_PUSHPULL:
        nrf_gpio_cfg_output(pin);
        break;
    case HF_GPIO_OUTPUT_OPENDRAIN:
        /* nrf_gpio_cfg_output() hardwires standard push-pull drive
         * (S0S1); open-drain needs the general nrf_gpio_cfg() with an
         * explicit disconnect-on-'1' drive strength (S0D1). */
        nrf_gpio_cfg(pin,
                     NRF_GPIO_PIN_DIR_OUTPUT,
                     NRF_GPIO_PIN_INPUT_DISCONNECT,
                     NRF_GPIO_PIN_NOPULL,
                     NRF_GPIO_PIN_S0D1,
                     NRF_GPIO_PIN_NOSENSE);
        break;
    default:
        return false;
    }
    return true;
}

void hf_gpio_write(hf_gpio_t pin, bool state)
{
    if (state) {
        nrf_gpio_pin_set(pin);
    } else {
        nrf_gpio_pin_clear(pin);
    }
}

bool hf_gpio_read(hf_gpio_t pin)
{
    return nrf_gpio_pin_read(pin) != 0;
}

void hf_gpio_toggle(hf_gpio_t pin)
{
    nrf_gpio_pin_toggle(pin);
}

#endif /* HF_ENABLE_GPIO */
#endif /* TARGET_NRF52 */
