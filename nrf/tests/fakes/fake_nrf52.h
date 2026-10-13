/* Test-control surface for tests/fakes/nrf52's fake nrfx. Tests call
 * these to script behavior, fire simulated hardware events into the
 * adapter's registered (and otherwise-private, static) event handlers,
 * and inspect what the adapter did.
 */
#ifndef HAL_WRAPPER_FAKE_NRF52_H
#define HAL_WRAPPER_FAKE_NRF52_H

#include "nrf_gpio.h"
#include "nrfx_uarte.h"
#include "nrfx_twim.h"
#include "nrfx_spim.h"
#include "nrfx_saadc.h"
#include "nrfx_timer.h"
#include "nrfx_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

void fake_nrf52_reset(void);

/* Shared "what should the next fake nrfx_* call return" knob - every
 * fake function in this vendor's fakes returns this unless it has a
 * more specific reason to differ. Defaults to NRFX_SUCCESS. */
extern nrfx_err_t fake_nrf52_next_status;

/* When true (the default), a successful kick-off (nrfx_uarte_tx,
 * nrfx_twim_xfer, nrfx_spim_xfer) immediately invokes the handler
 * registered for that instance with a DONE event, synchronously -
 * this is what makes this vendor's "kick off, then spin on our own
 * busy flag" blocking adapter functions terminate on a host machine.
 * Set to false before calling an _async function to control exactly
 * when completion happens via the fake_nrf52_*_fire_event helpers
 * instead (e.g. to check a callback hasn't fired yet, or to fire an
 * error event). */
extern bool fake_nrf52_auto_complete;

/* ---- GPIO ------------------------------------------------------------ */
void fake_nrf52_gpio_set_input(uint32_t pin, bool state);

/* ---- UART (nrfx_uarte) ------------------------------------------------ */
/* Fires a simulated event into whichever handler was registered by the
 * Nth (0-based) nrfx_uarte_init() call. Always invokes with
 * p_context == NULL - see the "REAL FINDING" comment in
 * nrfx_uarte.h for why. */
void fake_nrf52_uarte_fire_event(int slot, nrfx_uarte_event_t const *event);

/* ---- I2C (nrfx_twim) --------------------------------------------------- */
/* Controls whether fake_nrf52_auto_complete's synthesized event is
 * NRFX_TWIM_EVT_DONE (true, the default) or NRFX_TWIM_EVT_ADDRESS_NACK
 * (false) - lets a test make even a BLOCKING i2c call see a bus
 * failure, which a manually-fired event cannot do (the blocking call's
 * spin-wait has already claimed the only thread of execution). */
extern bool fake_nrf52_twim_auto_success;

/* Fires a simulated event into whichever handler was registered by the
 * Nth (0-based) nrfx_twim_init() call, using the context that call was
 * actually given (this vendor's I2C adapter passes one explicitly). */
void fake_nrf52_twim_fire_event(int slot, nrfx_twim_event_t const *event);

/* ---- SPI (nrfx_spim) --------------------------------------------------- */
void fake_nrf52_spim_fire_event(int slot, nrfx_spim_evt_t const *event);

/* ---- ADC (nrfx_saadc) --------------------------------------------------- */
/* Value nrfx_saadc_mode_trigger() writes into the sample buffer when
 * called in blocking mode (last simple_mode_set() call had a NULL
 * handler). */
extern nrf_saadc_value_t fake_nrf52_saadc_next_blocking_value;

/* Fires a DONE event, with the given sample, into whichever handler
 * the last nrfx_saadc_simple_mode_set() call registered (async mode). */
void fake_nrf52_saadc_fire_done(nrf_saadc_value_t sample);

/* ---- Timer (nrfx_timer) ------------------------------------------------- */
/* Fires event_type into whichever handler was registered by the Nth
 * (0-based, in first-init order) nrfx_timer_init() call, using the
 * p_context that call was actually given - this vendor's Timer
 * adapter passes its own instance number there. */
void fake_nrf52_timer_fire_event(int slot, nrf_timer_event_t event_type);

/* ---- PWM (nrfx_pwm) ------------------------------------------------------ */
/* Whether nrfx_pwm_simple_playback() was ever called for the Nth
 * (0-based, in first-init order) nrfx_pwm_init() instance, and whether
 * it's still "playing" (no nrfx_pwm_stop() since). */
bool fake_nrf52_pwm_is_playing(int slot);

/* The sequence pointer the adapter last handed to
 * nrfx_pwm_simple_playback()/nrfx_pwm_sequence_update() for the Nth
 * instance - dereference ->values.p_individual->channel_N directly to
 * assert on duty values the adapter wrote into its own static buffer. */
nrf_pwm_sequence_t const *fake_nrf52_pwm_last_sequence(int slot);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRF52_H */
