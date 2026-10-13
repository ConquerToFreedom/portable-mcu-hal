/* Silicon Labs adapter for the PWM portion of hal_wrapper.h, against
 * the current `sl_pwm` driver — gated by TARGET_SILABS_SLGPIO, the same
 * target macro the current sl_gpio/sl_i2c/IADC/sl_sleeptimer adapters
 * use. Verified sl_pwm_instance_t/sl_pwm_config_t/sl_pwm_init/start/
 * stop/set_duty_cycle directly against platform/driver/pwm/inc/sl_pwm.h
 * and platform/driver/pwm/src/sl_pwm.c in SiliconLabs/simplicity_sdk
 * (sisdk-2025.6) before writing this.
 *
 * A THIRD SHAPE for this vendor's current SDK, not a repeat of Timer's
 * or GPIO/I2C's precedent: unlike Timer (where the current SDK
 * replaced hardware-instance selection entirely with the software
 * service sl_sleeptimer), Silicon Labs does ship a real,
 * current-generation PWM driver here — but confirmed via
 * sl_pwm_instance_t's own fields (`TIMER_TypeDef *timer` sitting right
 * next to `channel`/`port`/`pin`/`location`) that it's a thin
 * convenience wrapper directly over the exact same classic emlib TIMER
 * hardware `hal_port_silabs_pwm.c` (TARGET_SILABS) uses, not a
 * genuinely different peripheral the way sl_gpio/sl_i2c are over
 * classic GPIO/I2C. Used anyway — it's a real driver Silicon Labs
 * ships and documents for this generation, same "use the vendor's
 * current driver, document its one real limitation" choice already
 * made for sl_gpio/sl_i2c/sl_sleeptimer.
 *
 * sl_pwm_init() confirmed (by reading sl_pwm.c directly) to handle
 * EVERYTHING this file would otherwise need to do by hand on the
 * classic side: it enables both the TIMER's own bus clock and the
 * GPIO bus clock internally (`sl_clock_manager_enable_bus_clock()`),
 * configures the output pin itself (`sl_gpio_set_pin_mode()`), and
 * writes the TIMER CC channel's GPIO->TIMERROUTE port/pin connection
 * itself — this file does none of the clock-enable or pin-routing work
 * hal_port_silabs_pwm.c has to (and only partially can) do by hand.
 * The ROUTEEN enable step happens inside sl_pwm_start(), confirmed
 * from the same source.
 *
 * `location` in sl_pwm_instance_t is a Series-1-era ROUTELOC concept —
 * left at 0 here since Series 2/3 (this target's actual scope) route
 * by plain port+pin, not a numbered location; verify against your SDK
 * if you're on a part old enough for `location` to matter.
 *
 * TIMER2/TIMER3, not TIMER0/TIMER1 — purely for consistency with
 * hal_port_silabs_pwm.c's own choice, not because of an actual
 * conflict on this target: hf_timer_t's TARGET_SILABS_SLGPIO adapter
 * uses sl_sleeptimer (RTCC/BURTC), entirely different hardware from
 * classic TIMER, so there is no real sharing risk here the way there
 * is on TARGET_SILABS.
 *
 * `freq_hz` is genuinely accurate here, not advisory (a real
 * improvement over the classic adapter's assumed-1MHz-prescale
 * situation): sl_pwm_config_t.frequency takes a plain Hz value and the
 * driver itself computes the correct TOP/prescale internally.
 *
 * duty_millipercent has a real, permanent resolution loss on this
 * target specifically: sl_pwm_set_duty_cycle() only takes a plain
 * uint8_t percent (0-100) — confirmed directly from sl_pwm.h — so this
 * file rounds duty_millipercent to the nearest whole percent, the same
 * category of finding as this SDK generation's sl_sleeptimer
 * millisecond-rounding for Timer.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_PWM

#include "sl_pwm.h"

#ifdef TIMER2
#define HF_SILABS_PWM_TIMER_0 TIMER2
#else
#define HF_SILABS_PWM_TIMER_0 NULL
#endif
#ifdef TIMER3
#define HF_SILABS_PWM_TIMER_1 TIMER3
#else
#define HF_SILABS_PWM_TIMER_1 NULL
#endif

static TIMER_TypeDef * const s_pwm_timer_table[] = { HF_SILABS_PWM_TIMER_0, HF_SILABS_PWM_TIMER_1 };
#define HF_SILABS_PWM_COUNT (sizeof(s_pwm_timer_table) / sizeof(s_pwm_timer_table[0]))

/* EDIT ME: match these to your board's actual PWM output wiring — see
 * file header comment. Placeholders, not verified board defaults. */
static const uint8_t s_pwm_port[HF_SILABS_PWM_COUNT][4] = {
    { 0, 0, 0, 0 }, /* TIMER2 CC0-3 — EDIT ME (0 = port A) */
    { 1, 1, 1, 1 }, /* TIMER3 CC0-3 — EDIT ME (1 = port B) */
};
static const uint8_t s_pwm_pin[HF_SILABS_PWM_COUNT][4] = {
    { 0, 1, 2, 3 }, /* TIMER2 CC0-3 — EDIT ME */
    { 0, 1, 2, 3 }, /* TIMER3 CC0-3 — EDIT ME */
};

static sl_pwm_instance_t s_pwm_instance[HF_SILABS_PWM_COUNT][4];
static bool s_valid[HF_SILABS_PWM_COUNT][4];

static sl_pwm_instance_t *hf_decode_pwm(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    uint32_t ch = channel & 0x3;
    if (instance >= HF_SILABS_PWM_COUNT || s_pwm_timer_table[instance] == NULL) {
        return NULL;
    }
    if (!s_valid[instance][ch]) {
        return NULL;
    }
    return &s_pwm_instance[instance][ch];
}

bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz)
{
    uint32_t instance = channel >> 2;
    uint32_t ch = channel & 0x3;
    if (instance >= HF_SILABS_PWM_COUNT || s_pwm_timer_table[instance] == NULL
        || freq_hz == 0U || ch >= 4U) {
        return false;
    }

    sl_pwm_instance_t *pwm = &s_pwm_instance[instance][ch];
    pwm->timer = s_pwm_timer_table[instance];
    pwm->channel = (uint8_t)ch;
    pwm->port = s_pwm_port[instance][ch];
    pwm->pin = s_pwm_pin[instance][ch];
    pwm->location = 0; /* Series 2/3 route by port+pin, not location — see file header comment */

    sl_pwm_config_t config = {0};
    config.frequency = (int)freq_hz; /* genuinely accurate — see file header comment */
    config.polarity = PWM_ACTIVE_HIGH;

    if (sl_pwm_init(pwm, &config) != SL_STATUS_OK) {
        return false;
    }
    s_valid[instance][ch] = true;
    return true;
}

bool hf_pwm_start(hf_pwm_t channel)
{
    sl_pwm_instance_t *pwm = hf_decode_pwm(channel);
    if (pwm == NULL) {
        return false;
    }
    sl_pwm_start(pwm);
    return true;
}

void hf_pwm_stop(hf_pwm_t channel)
{
    sl_pwm_instance_t *pwm = hf_decode_pwm(channel);
    if (pwm == NULL) {
        return;
    }
    sl_pwm_stop(pwm);
}

bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent)
{
    sl_pwm_instance_t *pwm = hf_decode_pwm(channel);
    if (pwm == NULL || duty_millipercent > 100000U) {
        return false;
    }
    /* Rounded to the nearest whole percent — real, permanent
     * resolution loss on this target, see file header comment. */
    uint8_t percent = (uint8_t)((duty_millipercent + 500U) / 1000U);
    if (percent > 100U) { percent = 100U; }
    sl_pwm_set_duty_cycle(pwm, percent);
    return true;
}

#endif /* HF_ENABLE_PWM */
#endif /* TARGET_SILABS_SLGPIO */
