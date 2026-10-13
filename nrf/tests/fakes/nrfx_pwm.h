/* Fake stand-in for nrfx's nrfx_pwm.h (plus hal/nrf_pwm.h's structs),
 * used ONLY to host-compile and unit-test this vendor's PWM adapter
 * under nrf/pwm/. Not a copy of Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_PWM_H
#define HAL_WRAPPER_FAKE_NRFX_PWM_H

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

typedef struct { uint8_t instance_id; } nrfx_pwm_t;
#define NRFX_PWM_INSTANCE(id) { .instance_id = (id) }

#define NRF_PWM_CHANNEL_COUNT 4

typedef enum { NRF_PWM_CLK_1MHz } nrf_pwm_clk_t;
typedef enum { NRF_PWM_MODE_UP } nrf_pwm_mode_t;
typedef enum { NRF_PWM_LOAD_INDIVIDUAL } nrf_pwm_dec_load_t;
typedef enum { NRF_PWM_STEP_AUTO } nrf_pwm_dec_step_t;

typedef struct {
    uint32_t output_pins[NRF_PWM_CHANNEL_COUNT];
    uint8_t irq_priority;
    nrf_pwm_clk_t base_clock;
    nrf_pwm_mode_t count_mode;
    uint16_t top_value;
    nrf_pwm_dec_load_t load_mode;
    nrf_pwm_dec_step_t step_mode;
} nrfx_pwm_config_t;

typedef struct {
    uint16_t channel_0;
    uint16_t channel_1;
    uint16_t channel_2;
    uint16_t channel_3;
} nrf_pwm_values_individual_t;

typedef union {
    nrf_pwm_values_individual_t const *p_individual;
    uint16_t const *p_raw;
} nrf_pwm_values_t;

typedef struct {
    nrf_pwm_values_t values;
    uint16_t length;
    uint32_t repeats;
    uint32_t end_delay;
} nrf_pwm_sequence_t;

#define NRF_PWM_VALUES_LENGTH(array) (sizeof(array) / 2UL)

#define NRFX_PWM_FLAG_LOOP (1U << 0)

typedef void (*nrfx_pwm_handler_t)(uint32_t event_type);

nrfx_err_t nrfx_pwm_init(nrfx_pwm_t *p_instance, nrfx_pwm_config_t const *p_config,
                          nrfx_pwm_handler_t handler, void *p_context);
uint32_t nrfx_pwm_simple_playback(nrfx_pwm_t *p_instance, nrf_pwm_sequence_t const *p_sequence,
                                   uint16_t playback_count, uint32_t flags);
void nrfx_pwm_sequence_update(nrfx_pwm_t *p_instance, uint8_t seq_id, nrf_pwm_sequence_t const *p_sequence);
bool nrfx_pwm_stop(nrfx_pwm_t *p_instance, bool wait_until_stopped);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_PWM_H */
