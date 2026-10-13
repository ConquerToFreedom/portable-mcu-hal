/* Fake stand-in for nrfx's nrfx_timer.h (plus hal/nrf_timer.h's
 * enums), used ONLY to host-compile and unit-test this vendor's Timer
 * adapter under nrf/timer/. Not a copy of Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_TIMER_H
#define HAL_WRAPPER_FAKE_NRFX_TIMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t instance_id; } nrfx_timer_t;
#define NRFX_TIMER_INSTANCE(id) { .instance_id = (id) }

typedef enum { NRF_TIMER_MODE_TIMER } nrf_timer_mode_t;
typedef enum { NRF_TIMER_BIT_WIDTH_32 } nrf_timer_bit_width_t;

typedef enum {
    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK = 1U << 0,
    NRF_TIMER_SHORT_COMPARE0_STOP_MASK  = 1U << 1
} nrf_timer_short_mask_t;

typedef enum { NRF_TIMER_EVENT_COMPARE0 } nrf_timer_event_t;

typedef struct {
    uint32_t frequency;
    nrf_timer_mode_t mode;
    nrf_timer_bit_width_t bit_width;
    uint8_t interrupt_priority;
    void *p_context;
} nrfx_timer_config_t;

typedef void (*nrfx_timer_event_handler_t)(nrf_timer_event_t event_type, void *p_context);

nrfx_err_t nrfx_timer_init(nrfx_timer_t *p_instance, nrfx_timer_config_t const *p_config,
                            nrfx_timer_event_handler_t handler);
uint32_t nrfx_timer_us_to_ticks(nrfx_timer_t *p_instance, uint32_t timer_us);
void nrfx_timer_extended_compare(nrfx_timer_t *p_instance, uint32_t channel, uint32_t value,
                                  nrf_timer_short_mask_t short_mask, bool enable_int);
void nrfx_timer_clear(nrfx_timer_t *p_instance);
void nrfx_timer_enable(nrfx_timer_t *p_instance);
void nrfx_timer_disable(nrfx_timer_t *p_instance);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_TIMER_H */
