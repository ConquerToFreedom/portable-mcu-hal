/* Silicon Labs (emlib) vendor adapter for the PWM portion of
 * hal_wrapper.h — the classic/legacy API, gated by TARGET_SILABS.
 * Verified TIMER_InitCC_TypeDef's fields directly against
 * SiliconLabs/gecko_sdk's platform/emlib/inc/em_timer.h (master)
 * before writing this — same header hf_timer_t's classic adapter
 * already uses TIMER_Init/TIMER_TopSet/TIMER_Enable from.
 *
 * THE ONE VENDOR WHERE hf_timer_t AND hf_pwm_t GENUINELY SHARE
 * HARDWARE — see hf_pwm_t's own header comment in hal_wrapper.h.
 * hf_timer_t's classic adapter already claims TIMER0/TIMER1; this file
 * deliberately uses TIMER2/TIMER3 instead. If your part only has two
 * TIMER blocks total, you cannot enable both hf_timer_t and hf_pwm_t
 * on TARGET_SILABS at once — trim whichever peripheral you don't need,
 * same "loud, not silent" precedent as every other instance-table
 * trim in this project.
 *
 * mode = timerCCModePWM plus cmoa = timerOutputActionToggle: the
 * `cmoa` (compare-match output action) pairing wasn't obvious from
 * em_timer.h's own struct comments alone (which only document `mode`
 * as the field that selects PWM), but is confirmed load-bearing by
 * reading Silicon Labs' own current-SDK sl_pwm driver source
 * (platform/driver/pwm/src/sl_pwm.c, SiliconLabs/simplicity_sdk,
 * sisdk-2025.6), which sets exactly this pairing for Series 2 devices
 * before generating real PWM output on the same TIMER hardware this
 * file targets. Not independently re-verified against a Series 0/1
 * reference manual — if your part predates Series 2, verify this
 * pairing still applies before relying on it.
 *
 * PIN ROUTING — the one genuinely uncertain part of this file. Classic
 * emlib's TIMER_InitCC() does not connect a CC channel's output to any
 * physical pin; that's a separate register write with a real
 * implementation split across Silicon Labs' own device generations:
 * - Series 2 with combined GPIO->TIMERROUTE (what this file
 *   implements, confirmed by reading sl_pwm.c's real source): write
 *   port/pin into `GPIO->TIMERROUTE[TIMER_NUM(timer)].CC0ROUTE +
 *   channel`, then set the matching bit in
 *   `GPIO->TIMERROUTE_SET[TIMER_NUM(timer)].ROUTEEN` to actually
 *   enable the route (done in hf_pwm_start(), not hf_pwm_init(), same
 *   split sl_pwm.c uses).
 * - Older Series 2 parts without the combined TIMERROUTE array use
 *   per-timer arrays instead (`GPIO->TIMER0ROUTE[0].CC0ROUTE` /
 *   `GPIO->TIMER0ROUTE_SET[0].ROUTEEN`, and similarly for TIMER2/3) —
 *   NOT implemented here; EDIT ME if your part needs this scheme
 *   instead (guarded by the absence of `_GPIO_TIMER_ROUTEEN_MASK`).
 * - Series 0/1 parts use a THIRD, entirely different mechanism on the
 *   TIMER peripheral itself (`TIMER_TypeDef`'s own `ROUTEPEN`/
 *   `ROUTELOC0` fields, no `GPIO->TIMERROUTE` involved at all) — NOT
 *   implemented here at all; EDIT ME for that device family.
 * Cross-check which scheme your part actually uses against its
 * reference manual before relying on this file, same "flag what's
 * genuinely uncertain rather than guess" treatment as this vendor's
 * IADC/TI's most heavily-flagged adapters elsewhere in this repo.
 *
 * `freq_hz` is advisory in the same sense as hf_timer_t's classic
 * adapter: turning it into an exact TOP value assumes a 1 MHz
 * HFPERCLK-after-prescale tick rate (prescale left at `timerPrescale1`)
 * — EDIT the `prescale` field below if your part's HFPERCLK isn't
 * already 1 MHz.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_PWM

#include "em_timer.h"
#include "em_cmu.h"
#include "em_gpio.h"

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
    { gpioPortA, gpioPortA, gpioPortA, gpioPortA }, /* TIMER2 CC0-3 — EDIT ME */
    { gpioPortB, gpioPortB, gpioPortB, gpioPortB }, /* TIMER3 CC0-3 — EDIT ME */
};
static const uint8_t s_pwm_pin[HF_SILABS_PWM_COUNT][4] = {
    { 0, 1, 2, 3 }, /* TIMER2 CC0-3 — EDIT ME */
    { 0, 1, 2, 3 }, /* TIMER3 CC0-3 — EDIT ME */
};

static uint32_t s_top[HF_SILABS_PWM_COUNT];

static TIMER_TypeDef *hf_decode_pwm_timer(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    if (instance >= HF_SILABS_PWM_COUNT || s_pwm_timer_table[instance] == NULL) {
        return NULL;
    }
    return s_pwm_timer_table[instance];
}

#if defined(_GPIO_TIMER_ROUTEEN_MASK)
static int hf_pwm_timer_num(TIMER_TypeDef *timer)
{
#ifdef TIMER2
    if (timer == TIMER2) { return TIMER_NUM(TIMER2); }
#endif
#ifdef TIMER3
    if (timer == TIMER3) { return TIMER_NUM(TIMER3); }
#endif
    return -1;
}
#endif

bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz)
{
    uint32_t instance = channel >> 2;
    uint32_t ch = channel & 0x3;
    TIMER_TypeDef *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL || freq_hz == 0U || ch >= 4U) {
        return false;
    }

#ifdef TIMER2
    if (timer == TIMER2) { CMU_ClockEnable(cmuClock_TIMER2, true); }
#endif
#ifdef TIMER3
    if (timer == TIMER3) { CMU_ClockEnable(cmuClock_TIMER3, true); }
#endif

    /* Assumes 1 MHz HFPERCLK-after-prescale — see file header comment,
     * EDIT the prescale field otherwise. */
    uint32_t top = (1000000U / freq_hz);
    if (top > 0U) { top -= 1U; }
    s_top[instance] = top + 1U;
    TIMER_TopSet(timer, top);

    TIMER_InitCC_TypeDef ccInit = TIMER_INITCC_DEFAULT;
    ccInit.mode = timerCCModePWM;
    ccInit.cmoa = timerOutputActionToggle; /* confirmed via sl_pwm.c — see file header comment */
    TIMER_InitCC(timer, ch, &ccInit);

#if defined(_GPIO_TIMER_ROUTEEN_MASK)
    int timer_num = hf_pwm_timer_num(timer);
    if (timer_num >= 0) {
        volatile uint32_t *route = &GPIO->TIMERROUTE[timer_num].CC0ROUTE;
        route[ch] = ((uint32_t)s_pwm_port[instance][ch] << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
                  | ((uint32_t)s_pwm_pin[instance][ch] << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
    }
#endif

    TIMER_Init_TypeDef init = TIMER_INIT_DEFAULT;
    init.enable = false; /* configure only — hf_pwm_start() enables counting */
    TIMER_Init(timer, &init);
    return true;
}

bool hf_pwm_start(hf_pwm_t channel)
{
    TIMER_TypeDef *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL) {
        return false;
    }
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
    int timer_num = hf_pwm_timer_num(timer);
    if (timer_num >= 0) {
        GPIO->TIMERROUTE_SET[timer_num].ROUTEEN = 1U << ((channel & 0x3) + _GPIO_TIMER_ROUTEEN_CC0PEN_SHIFT);
    }
#endif
    TIMER_Enable(timer, true);
    return true;
}

void hf_pwm_stop(hf_pwm_t channel)
{
    TIMER_TypeDef *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL) {
        return;
    }
    TIMER_Enable(timer, false);
}

bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent)
{
    uint32_t instance = channel >> 2;
    uint32_t ch = channel & 0x3;
    TIMER_TypeDef *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL || duty_millipercent > 100000U) {
        return false;
    }
    uint32_t compare = (uint32_t)(((uint64_t)duty_millipercent * s_top[instance]) / 100000U);
    /* Buffered, not immediate — takes effect on the next update event,
     * so a duty change never glitches mid-period. */
    TIMER_CompareBufSet(timer, ch, compare);
    return true;
}

#endif /* HF_ENABLE_PWM */
#endif /* TARGET_SILABS */
