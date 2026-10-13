/* Fake stand-in for nrfx's nrfx_saadc.h (plus nrf_saadc.h's enums),
 * used ONLY to host-compile and unit-test this vendor's ADC adapter
 * under nrf/adc/. Not a copy of Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_SAADC_H
#define HAL_WRAPPER_FAKE_NRFX_SAADC_H

#include <stdint.h>
#include <stddef.h>

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t nrf_saadc_value_t;

typedef enum {
    NRF_SAADC_INPUT_AIN0, NRF_SAADC_INPUT_AIN1, NRF_SAADC_INPUT_AIN2, NRF_SAADC_INPUT_AIN3,
    NRF_SAADC_INPUT_AIN4, NRF_SAADC_INPUT_AIN5, NRF_SAADC_INPUT_AIN6, NRF_SAADC_INPUT_AIN7
} nrfx_analog_input_t;

typedef enum {
    NRF_SAADC_RESOLUTION_8BIT, NRF_SAADC_RESOLUTION_10BIT,
    NRF_SAADC_RESOLUTION_12BIT, NRF_SAADC_RESOLUTION_14BIT
} nrf_saadc_resolution_t;

typedef enum { NRF_SAADC_OVERSAMPLE_DISABLED } nrf_saadc_oversample_t;

typedef struct { nrfx_analog_input_t pin_p; } nrfx_saadc_channel_t;
#define NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(pin) { .pin_p = (pin) }

typedef enum { NRFX_SAADC_EVT_DONE, NRFX_SAADC_EVT_CALIBRATEDONE } nrfx_saadc_evt_type_t;

typedef struct {
    nrf_saadc_value_t *p_buffer;
    uint16_t size;
} nrfx_saadc_done_evt_t;

typedef struct {
    nrfx_saadc_evt_type_t type;
    union {
        nrfx_saadc_done_evt_t done;
    } data;
} nrfx_saadc_evt_t;

typedef void (*nrfx_saadc_event_handler_t)(nrfx_saadc_evt_t const *p_event);

nrfx_err_t nrfx_saadc_init(uint8_t irq_priority);
nrfx_err_t nrfx_saadc_channel_config(nrfx_saadc_channel_t const *p_config);
nrfx_err_t nrfx_saadc_simple_mode_set(uint32_t channel_mask, nrf_saadc_resolution_t resolution,
                                      nrf_saadc_oversample_t oversampling, nrfx_saadc_event_handler_t handler);
nrfx_err_t nrfx_saadc_buffer_set(nrf_saadc_value_t *p_buffer, uint16_t size);
nrfx_err_t nrfx_saadc_mode_trigger(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_SAADC_H */
