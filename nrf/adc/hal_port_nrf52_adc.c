/* nRF52 (nrfx) vendor adapter for the ADC portion of hal_wrapper.h,
 * against nrfx_saadc (Successive Approximation ADC with EasyDMA).
 * Verified nrfx_saadc.h (and nrf_saadc.h for the resolution enum)
 * directly before writing this.
 *
 * Only one SAADC peripheral exists on nRF52 (832 and 840 alike) —
 * unlike this vendor's I2C/UART/SPI adapters, which each cover two
 * TWIM/UARTE/SPIM instances, hf_adc_t only ever has one valid value
 * here (0).
 *
 * Genuine finding, the opposite of this vendor's I2C/UART/SPI
 * situation: SAADC actually has a native blocking mode.
 * nrfx_saadc_simple_mode_set() called with a NULL event handler makes
 * nrfx_saadc_mode_trigger() block until the conversion finishes — so
 * unlike SPIM/TWIM/UARTE (none of which have a NULL-handler blocking
 * mode, forcing "blocking" to be implemented as kick-off-plus-spin-
 * wait on this file's own busy flag), hf_adc_read() here calls
 * straight into the driver with no busy-flag choreography needed, and
 * only the async path touches s_busy/s_value_ptr at all.
 *
 * Channel-to-pin mapping is a fixed 1:1 relationship on this
 * peripheral (channel N is always NRF_SAADC_INPUT_AINn) — no EDIT-ME
 * board-wiring table needed the way this vendor's I2C/SPI SCL/SDA/SCK
 * pin tables are, since an ADC input doesn't have a customer-chosen
 * mux step the way a general-purpose GPIO-routed peripheral does.
 *
 * Two specifics flagged as not independently re-confirmed in this
 * fetch pass, following this vendor's own established honesty
 * convention: (1) NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(pin), the
 * single-ended default channel-config helper macro — a
 * near-universal convention in published SAADC examples, but not
 * re-quoted directly from the header text this pass returned; (2) the
 * exact success/failure return convention of nrfx_saadc_init() itself
 * — this file checks it against NRFX_SUCCESS for consistency with
 * every other nrfx call in this file and this vendor's other
 * adapters, but the specific fetch for this function described a
 * plain-integer/negative-errno-style return instead, which may
 * reflect a newer nrfx build configuration than the rest of this file
 * assumes. Verify both against your installed nrfx version.
 *
 * IRQ priority is a hardcoded literal rather than pulled from a
 * customer's sdk_config.h, consistent with this project's other
 * adapters not depending on a vendor SDK's generated config file —
 * edit it if your project needs a different priority.
 *
 * Every call reconfigures the one hardware channel used (channel
 * index 0 within the driver) before triggering a conversion, since v1
 * only ever runs one logical ADC channel per hf_adc_read()/
 * read_async() call.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_ADC

#include <stdint.h>
#include "nrfx_saadc.h"

#define HF_NRF52_ADC_IRQ_PRIORITY 6

static const nrfx_analog_input_t s_adc_pins[] = {
    NRF_SAADC_INPUT_AIN0, NRF_SAADC_INPUT_AIN1,
    NRF_SAADC_INPUT_AIN2, NRF_SAADC_INPUT_AIN3,
    NRF_SAADC_INPUT_AIN4, NRF_SAADC_INPUT_AIN5,
    NRF_SAADC_INPUT_AIN6, NRF_SAADC_INPUT_AIN7,
};
#define HF_NRF52_ADC_CHANNEL_COUNT (sizeof(s_adc_pins) / sizeof(s_adc_pins[0]))

static bool s_saadc_initialized;
static volatile bool s_busy;
static uint16_t *s_value_ptr;
static hf_adc_callback_t s_callback;
static nrf_saadc_value_t s_async_sample;

static void hf_adc_event_handler(nrfx_saadc_evt_t const *p_event)
{
    if (p_event->type != NRFX_SAADC_EVT_DONE) {
        return;
    }
    if (s_value_ptr != NULL) {
        *s_value_ptr = (uint16_t)p_event->data.done.p_buffer[0];
    }
    s_busy = false;
    if (s_callback != NULL) {
        s_callback(0);
    }
}

bool hf_adc_init(hf_adc_t instance)
{
    if (instance != 0) {
        return false;
    }
    if (s_saadc_initialized) {
        return true;
    }
    if (nrfx_saadc_init(HF_NRF52_ADC_IRQ_PRIORITY) != NRFX_SUCCESS) {
        return false;
    }
    s_saadc_initialized = true;
    return true;
}

static bool hf_adc_configure_channel(hf_adc_channel_t channel)
{
    if (channel >= HF_NRF52_ADC_CHANNEL_COUNT) {
        return false;
    }
    nrfx_saadc_channel_t channel_cfg = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(s_adc_pins[channel]);
    return nrfx_saadc_channel_config(&channel_cfg) == NRFX_SUCCESS;
}

bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    if (instance != 0 || !hf_adc_configure_channel(channel)) {
        return false;
    }
    if (nrfx_saadc_simple_mode_set(1UL << 0, NRF_SAADC_RESOLUTION_12BIT,
                                    NRF_SAADC_OVERSAMPLE_DISABLED, NULL) != NRFX_SUCCESS) {
        return false;
    }
    nrf_saadc_value_t sample;
    if (nrfx_saadc_buffer_set(&sample, 1) != NRFX_SUCCESS) {
        return false;
    }
    if (nrfx_saadc_mode_trigger() != NRFX_SUCCESS) {
        return false;
    }
    *value = (uint16_t)sample;
    return true;
}

void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback)
{
    if (instance == 0) {
        s_callback = callback;
    }
}

bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    if (instance != 0 || s_busy || !hf_adc_configure_channel(channel)) {
        return false;
    }
    if (nrfx_saadc_simple_mode_set(1UL << 0, NRF_SAADC_RESOLUTION_12BIT,
                                    NRF_SAADC_OVERSAMPLE_DISABLED,
                                    hf_adc_event_handler) != NRFX_SUCCESS) {
        return false;
    }
    s_value_ptr = value;
    s_busy = true;
    if (nrfx_saadc_buffer_set(&s_async_sample, 1) != NRFX_SUCCESS) {
        s_busy = false;
        return false;
    }
    if (nrfx_saadc_mode_trigger() != NRFX_SUCCESS) {
        s_busy = false;
        return false;
    }
    return true;
}

#endif /* HF_ENABLE_ADC */
#endif /* TARGET_NRF52 */
