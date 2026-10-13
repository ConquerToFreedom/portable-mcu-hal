/* Test-control surface for tests/fakes/silabs_slgpio's fake current
 * Silicon Labs drivers (sl_gpio, sl_i2c, IADC - TARGET_SILABS_SLGPIO).
 * UART and SPI on this target share fake_silabs.c's fakes instead (see
 * that file) since the real adapters share the same underlying
 * classic USART/SPIDRV hardware drivers - link both fakes together
 * when building a TARGET_SILABS_SLGPIO test binary.
 */
#ifndef HAL_WRAPPER_FAKE_SILABS_SLGPIO_H
#define HAL_WRAPPER_FAKE_SILABS_SLGPIO_H

#include "sl_gpio.h"
#include "sl_i2c.h"
#include "em_iadc.h"
#include "sl_sleeptimer.h"
#include "sl_pwm.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void fake_silabs_slgpio_reset(void);

/* ---- GPIO (sl_gpio) ------------------------------------------------------ */
void fake_silabs_slgpio_gpio_set_input(uint8_t port, uint8_t pin, bool state);

/* ---- I2C (sl_i2c) ---------------------------------------------------------- */
extern sl_status_t fake_sli2c_next_status;
/* When true (default), a successful send/receive_non_blocking call
 * immediately invokes its callback synchronously with
 * fake_sli2c_next_event. Set false to fire manually via
 * fake_sli2c_fire_event() instead. */
extern bool fake_sli2c_auto_complete;
extern sl_i2c_event_t fake_sli2c_next_event;
void fake_sli2c_fire_event(sl_i2c_event_t event);

/* ---- ADC (IADC) ------------------------------------------------------------ */
extern uint32_t fake_iadc_next_value;

/* ---- Timer (sl_sleeptimer) --------------------------------------------------- */
/* Fires the callback registered by the Nth (0-based, in start order)
 * sl_sleeptimer_start_timer()/start_periodic_timer() call - simulates
 * the tick elapsing. */
void fake_sl_sleeptimer_fire(int slot);
/* Whether sl_sleeptimer_init() has been called since the last reset -
 * regression coverage for the real bug this adapter once had (it never
 * called sl_sleeptimer_init() at all; see docs/architecture.md's
 * Silicon Labs clock-enable bug-fix section). */
bool fake_sl_sleeptimer_is_initialized(void);

/* ---- PWM (sl_pwm) ------------------------------------------------------------- */
/* sl_pwm_instance_t is adapter-private state (a static array inside
 * hal_port_silabs_slgpio_pwm.c) - these accessors identify an instance
 * by slot number instead: the Nth (0-based) distinct sl_pwm_init()
 * call, in the order hf_pwm_init() made them. As long as a test calls
 * hf_pwm_init() in a known order, slot 0 is always its first call,
 * slot 1 its second, and so on - same pattern as this vendor's
 * fake_nrf52.h uses for nrfx_pwm's per-instance state. */
uint8_t fake_sl_pwm_get_duty_cycle(int slot);
/* Whether sl_pwm_start() was called for the given slot and no
 * sl_pwm_stop() has followed it since. */
bool fake_sl_pwm_is_running(int slot);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SILABS_SLGPIO_H */
