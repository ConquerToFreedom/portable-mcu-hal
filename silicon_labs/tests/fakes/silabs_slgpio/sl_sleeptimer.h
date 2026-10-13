/* Fake stand-in for Silicon Labs' current sl_sleeptimer.h (Simplicity
 * SDK's software timer service), used ONLY to host-compile and
 * unit-test this vendor's current Timer adapter under
 * silicon_labs/timer/hal_port_silabs_slgpio_timer.c
 * (TARGET_SILABS_SLGPIO). Not a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_SL_SLEEPTIMER_H
#define HAL_WRAPPER_FAKE_SL_SLEEPTIMER_H

#include <stdint.h>
#include <stddef.h>
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int _opaque; } sl_sleeptimer_timer_handle_t;

typedef void (*sl_sleeptimer_timer_callback_t)(sl_sleeptimer_timer_handle_t *handle, void *data);

sl_status_t sl_sleeptimer_init(void);
sl_status_t sl_sleeptimer_ms32_to_tick(uint32_t time_ms, uint32_t *tick);
sl_status_t sl_sleeptimer_start_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout,
                                       sl_sleeptimer_timer_callback_t callback, void *callback_data,
                                       uint8_t priority, uint16_t option_flags);
sl_status_t sl_sleeptimer_start_periodic_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout,
                                                sl_sleeptimer_timer_callback_t callback, void *callback_data,
                                                uint8_t priority, uint16_t option_flags);
sl_status_t sl_sleeptimer_stop_timer(sl_sleeptimer_timer_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SL_SLEEPTIMER_H */
