/* portable-mcu-hal hardware verification: EFR32 "smoke" test.
 *
 * Hand-written for this verification pipeline — NOT copied from a
 * Silicon Labs example (this sandbox has no network path to
 * silabs.com to fetch one; see docs/verification/efr32.md). Per the
 * project's hard rule ("keep original license headers" on anything
 * actually copied from a vendor example), this file carries no vendor
 * license header because none of its content came from Silicon Labs
 * source — do not add one speculatively.
 *
 * Intended final shape (to confirm/adjust against a real `slc
 * generate --with brd2601b` run in CI, which does reach silabs.com):
 * this file replaces the main.c of a project generated from one of
 * Simplicity SDK's own board-support/LED example apps for BRD2601B —
 * keeping that generated project's board init, clock setup, and
 * pin-config headers, discarding its own LED-blink application logic.
 * This test drives the LED through the wrapper's own hf_gpio_* API,
 * not the SDK's `sl_led_*`/`sl_simple_led` driver, so a wrapper GPIO
 * bug can't hide behind the SDK's own LED driver working correctly.
 *
 * NOT YET CONFIRMED (first real CI run is expected to need a small fix
 * here, flagged rather than hidden, same convention this project uses
 * throughout for unconfirmed vendor specifics — see TI MSPM0 adapters'
 * own header comments for the precedent): the exact header name and
 * macro names the generated project's `autogen/sl_simple_led_
 * instances.h` (or equivalent) exposes for LED0's port/pin. The
 * fallback `#define`s below are a placeholder guess, not a verified
 * real symbol.
 */
#include <stdio.h>

#include "sl_system_init.h"
#include "sl_simple_led_instances.h" /* EDIT-ME: confirm this header name once generated */

#include "hal_wrapper.h"

#ifndef FW_GIT_SHA
#error "FW_GIT_SHA must be defined by the build (scripts/build.sh passes -DFW_GIT_SHA=\"<short_sha>\")"
#endif

/* EDIT-ME: confirm these two macro names (or substitute the generated
 * project's real ones) once `slc generate --with brd2601b` has
 * actually run — see the file header comment above. */
#ifndef SL_SIMPLE_LED_LED0_PORT
#define SL_SIMPLE_LED_LED0_PORT 1 /* guessed: port B */
#endif
#ifndef SL_SIMPLE_LED_LED0_PIN
#define SL_SIMPLE_LED_LED0_PIN 2 /* guessed */
#endif

#define SMOKE_LED_PIN HF_PIN(SL_SIMPLE_LED_LED0_PORT, SL_SIMPLE_LED_LED0_PIN)
#define SMOKE_TOGGLE_COUNT 5

int main(void)
{
    sl_system_init();

    printf("[BOOT] fw=%s vendor=efr32 test=smoke\r\n", FW_GIT_SHA);

    bool ok = hf_gpio_init(SMOKE_LED_PIN, HF_GPIO_OUTPUT_PUSHPULL);
    printf("[GPIO] init pin=0x%02x val=na ret=%s\r\n", (unsigned)SMOKE_LED_PIN, ok ? "OK" : "ERR");

    for (int i = 0; i < SMOKE_TOGGLE_COUNT; i++) {
        hf_gpio_toggle(SMOKE_LED_PIN);
        bool state = hf_gpio_read(SMOKE_LED_PIN);
        printf("[GPIO] toggle pin=0x%02x val=%d ret=OK\r\n", (unsigned)SMOKE_LED_PIN, state ? 1 : 0);
    }

    printf("[APP] toggles=%d\r\n", SMOKE_TOGGLE_COUNT);
    printf("[DONE] test=smoke\r\n");

    while (1) {
        /* idle — HF·NODE's log capture stops reading at [DONE] above */
    }
}
