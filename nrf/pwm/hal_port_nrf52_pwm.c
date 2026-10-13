/* nRF52 vendor adapter for the PWM portion of hal_wrapper.h, against
 * nrfx_pwm — gated by TARGET_NRF52 (covers both nRF5 SDK and nRF
 * Connect SDK unchanged, same reasoning as this vendor's other
 * adapters).
 *
 * Structurally different from every other adapter in this repo, not
 * just a different vendor API: nrfx_pwm is an EasyDMA sequence player,
 * not a "write a compare register" peripheral. Verified
 * nrfx_pwm_config_t/nrfx_pwm_init/nrfx_pwm_simple_playback/
 * nrfx_pwm_sequence_update directly against
 * drivers/include/nrfx_pwm.h, and nrf_pwm_sequence_t/
 * nrf_pwm_values_individual_t's exact fields against hal/nrf_pwm.h, in
 * SiliconLabs/nrfx... no, NordicSemiconductor/nrfx (master) before
 * writing this.
 *
 * A "sequence" is a RAM-resident array of duty values that EasyDMA
 * reads from continuously while playing — NOT a one-shot snapshot the
 * hardware copies in at start. That means the array must stay valid
 * for as long as the channel is running, so this file keeps one
 * `nrf_pwm_values_individual_t` (4 named uint16_t fields — channel_0..
 * channel_3, not an array, per hal/nrf_pwm.h) as static storage per
 * PWM instance, not a stack-local one. `NRFX_PWM_FLAG_LOOP` is what
 * makes a single set of 4 duty values repeat indefinitely without CPU
 * intervention once started; `nrfx_pwm_sequence_update()` re-triggers
 * playback with the same array after `hf_pwm_set_duty()` changes one
 * field — this is nRF52's equivalent of every other vendor's
 * "glitch-free live compare update", just shaped as "restart playback
 * of the (now-modified) buffer" instead of a plain register write.
 *
 * Two PWM instances (PWM0, PWM1) — an entirely separate peripheral
 * numbering from NRFX_TIMER_INSTANCE's TIMER0/1/2 that hf_timer_t
 * uses, since nrfx_pwm and nrfx_timer are genuinely separate hardware
 * blocks (see hf_pwm_t's own header comment in hal_wrapper.h) — there
 * is no shared-instance conflict to avoid here the way Silicon Labs
 * classic has.
 *
 * EDIT ME: output_pins per instance/channel. Unlike I2C/SPI/UART,
 * nrfx_pwm has no separate pin-mux step to rely on and no handle this
 * file can take by extern the way STM32's CubeMX-configured timers
 * work — nrfx_pwm_config_t wants the actual pin numbers at init time,
 * same "no pre-muxing step, EDIT ME" situation as this vendor's I2C
 * adapter's SCL/SDA pin table. Placeholder pin numbers below are NOT
 * verified board defaults.
 *
 * `freq_hz` is genuinely accurate here, not advisory (matching
 * hf_timer_t's own nRF52 finding for the same reason): this project
 * owns the PWM clock configuration outright (fixed at
 * NRF_PWM_CLK_1MHz), so `top_value = 1000000 / freq_hz` is an exact
 * tick-rate conversion, not a guess about the customer's own clock
 * tree. Valid for freq_hz roughly 31 Hz to 1 MHz (top_value must fit
 * nrf_pwm_sequence_t's 15-bit range); frequencies outside that range
 * aren't validated here — same "advisory only where a limit is real
 * but not enforced in v1" treatment already accepted elsewhere in this
 * project.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_PWM

#include <stdint.h>
#include "nrfx_pwm.h"

/* Project-defined, not a vendor default macro — same "own constant,
 * not an assumed SDK default" precedent as hf_timer_t's nRF52 adapter
 * uses for HF_NRF52_TIMER_IRQ_PRIORITY. */
#define HF_NRF52_PWM_IRQ_PRIORITY 6

static nrfx_pwm_t s_pwm_instances[] = {
    NRFX_PWM_INSTANCE(0),
    NRFX_PWM_INSTANCE(1),
};
#define HF_NRF52_PWM_COUNT (sizeof(s_pwm_instances) / sizeof(s_pwm_instances[0]))

/* EDIT ME: match these to your board's actual PWM output wiring — see
 * file header comment. Placeholders, not verified board defaults. */
static const uint32_t s_output_pins[HF_NRF52_PWM_COUNT][NRF_PWM_CHANNEL_COUNT] = {
    { 3, 4, 28, 29 },   /* PWM0 channels 0-3 — EDIT ME */
    { 30, 31, 2, 5 },   /* PWM1 channels 0-3 — EDIT ME */
};

static nrf_pwm_values_individual_t s_duty[HF_NRF52_PWM_COUNT];
static nrf_pwm_sequence_t s_sequence[HF_NRF52_PWM_COUNT];
static uint16_t s_top_value[HF_NRF52_PWM_COUNT];
static bool s_initialized[HF_NRF52_PWM_COUNT];
static bool s_running[HF_NRF52_PWM_COUNT];

static nrfx_pwm_t *hf_decode_pwm_instance(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    if (instance >= HF_NRF52_PWM_COUNT) {
        return NULL;
    }
    return &s_pwm_instances[instance];
}

static void hf_pwm_write_channel(nrf_pwm_values_individual_t *values, uint32_t ch, uint16_t value)
{
    switch (ch) {
    case 0: values->channel_0 = value; break;
    case 1: values->channel_1 = value; break;
    case 2: values->channel_2 = value; break;
    default: values->channel_3 = value; break;
    }
}

bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz)
{
    uint32_t instance = channel >> 2;
    nrfx_pwm_t *pwm = hf_decode_pwm_instance(channel);
    if (pwm == NULL || freq_hz == 0U) {
        return false;
    }

    /* freq_hz is genuinely exact here — see file header comment. */
    uint32_t top_value = 1000000U / freq_hz;
    if (top_value == 0U) {
        top_value = 1U;
    }

    nrfx_pwm_config_t config = {0};
    config.output_pins[0] = s_output_pins[instance][0];
    config.output_pins[1] = s_output_pins[instance][1];
    config.output_pins[2] = s_output_pins[instance][2];
    config.output_pins[3] = s_output_pins[instance][3];
    config.irq_priority = HF_NRF52_PWM_IRQ_PRIORITY;
    config.base_clock = NRF_PWM_CLK_1MHz;
    config.count_mode = NRF_PWM_MODE_UP;
    config.top_value = (uint16_t)top_value;
    config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    config.step_mode = NRF_PWM_STEP_AUTO;

    if (nrfx_pwm_init(pwm, &config, NULL, NULL) != NRFX_SUCCESS) {
        return false;
    }
    s_top_value[instance] = (uint16_t)top_value;

    s_duty[instance].channel_0 = 0;
    s_duty[instance].channel_1 = 0;
    s_duty[instance].channel_2 = 0;
    s_duty[instance].channel_3 = 0;

    s_sequence[instance].values.p_individual = &s_duty[instance];
    s_sequence[instance].length = NRF_PWM_VALUES_LENGTH(s_duty[instance]);
    s_sequence[instance].repeats = 0;
    s_sequence[instance].end_delay = 0;

    s_initialized[instance] = true;
    s_running[instance] = false;
    return true;
}

bool hf_pwm_start(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    nrfx_pwm_t *pwm = hf_decode_pwm_instance(channel);
    if (pwm == NULL || !s_initialized[instance]) {
        return false;
    }
    (void)nrfx_pwm_simple_playback(pwm, &s_sequence[instance], 1, NRFX_PWM_FLAG_LOOP);
    s_running[instance] = true;
    return true;
}

void hf_pwm_stop(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    nrfx_pwm_t *pwm = hf_decode_pwm_instance(channel);
    if (pwm == NULL) {
        return;
    }
    (void)nrfx_pwm_stop(pwm, true);
    s_running[instance] = false;
}

bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent)
{
    uint32_t instance = channel >> 2;
    nrfx_pwm_t *pwm = hf_decode_pwm_instance(channel);
    if (pwm == NULL || !s_initialized[instance] || duty_millipercent > 100000U) {
        return false;
    }

    uint16_t raw = (uint16_t)(((uint64_t)duty_millipercent * s_top_value[instance]) / 100000U);
    hf_pwm_write_channel(&s_duty[instance], channel & 0x3, raw);

    if (s_running[instance]) {
        nrfx_pwm_sequence_update(pwm, 0, &s_sequence[instance]);
    }
    return true;
}

#endif /* HF_ENABLE_PWM */
#endif /* TARGET_NRF52 */
