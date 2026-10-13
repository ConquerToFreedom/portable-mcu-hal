/* TI MSPM0 (driverlib) vendor adapter for the PWM portion of
 * hal_wrapper.h, against DL_TimerA — the advanced/capture-compare-and-
 * PWM-capable timer family this vendor's Timer adapter deliberately
 * left free by choosing DL_TimerG for hf_timer_t instead. Disjoint by
 * design, not by luck: see hal_port_ti_mspm0_timer.c's own header
 * comment and hf_pwm_t's header comment in hal_wrapper.h. Assumes the
 * customer's SysConfig-generated "ti_msp_dl_config.h" is on the include
 * path, same assumption as this vendor's other adapters.
 *
 * Verified DL_Timer_PWMConfig's field list (period, pwmMode,
 * isTimerWithFourCC, startTimer) and DL_Timer_initFourCCPWMMode's
 * exact signature — (GPTIMER_Regs *gptimer, const DL_Timer_PWMConfig
 * *config) — directly against TexasInstruments/mspm0-sdk's
 * source/ti/driverlib/dl_timer.h/.c (needed the .c source for the real
 * prototype, same gap this vendor's Timer adapter hit for
 * DL_Timer_setCaptureCompareValue). DL_TimerA_initPWMMode is
 * confirmed to be a #define alias for DL_Timer_initFourCCPWMMode, so
 * this file calls the common function directly, same "shared function,
 * timer-family-specific macro is just a rename" pattern already
 * relied on for DL_TimerA_setCaptureCompareValue.
 *
 * DL_Timer_setCaptureCompareValue(gptimer, value, ccIndex) — confirmed
 * 3-argument signature, same source — is this vendor's equivalent of
 * every other vendor's "change duty without a glitch or re-init" call,
 * used here for hf_pwm_set_duty() exactly like TIMER_CompareBufSet()/
 * __HAL_TIM_SET_COMPARE() are for Silicon Labs classic/STM32.
 *
 * Several specifics carried over unchanged from this vendor's Timer
 * adapter, same flagged-not-guessed treatment — cross-check against
 * your installed MSPM0-SDK version's dl_timer.h before relying on this
 * in production:
 *   1. DL_TIMER_CLOCK_BUSCLK — assumed to exist and select the bus
 *      clock; the enum's full member list wasn't reached in either
 *      this or Timer's fetch pass.
 *   2. DL_Timer_PWMConfig.startTimer — passed a value assumed to mean
 *      "don't auto-start" (matching this project's init-configures-
 *      without-starting contract), inferred from the field's name and
 *      purpose and from Timer's own DL_Timer_TimerConfig.startTimer
 *      field sharing the same name, not independently re-confirmed for
 *      the PWM-config struct specifically.
 *   3. DL_TIMER_CC_0_INDEX.._3_INDEX — confirmed to exist as the
 *      channel-index constants DL_Timer_setCaptureCompareValue takes,
 *      but this file's assumption that they're numbered 0-3 in the
 *      straightforward order this adapter's channel encoding expects
 *      wasn't independently re-verified against their actual integer
 *      values.
 *
 * `freq_hz` is advisory in the same sense as this vendor's Timer
 * adapter's `period_us`: turning it into an accurate period value
 * assumes the bus clock (after DL_Timer_ClockConfig.divideRatio/
 * prescale) yields a 1 MHz tick rate — EDIT the clockConfig fields
 * below if your project's bus clock isn't already 1 MHz post-divide.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_PWM

#include "ti_msp_dl_config.h"

#ifdef TIMA0
#define HF_MSPM0_PWM_TIMER_0 TIMA0
#else
#define HF_MSPM0_PWM_TIMER_0 NULL
#endif
#ifdef TIMA1
#define HF_MSPM0_PWM_TIMER_1 TIMA1
#else
#define HF_MSPM0_PWM_TIMER_1 NULL
#endif

static GPTIMER_Regs * const s_pwm_timer_table[] = { HF_MSPM0_PWM_TIMER_0, HF_MSPM0_PWM_TIMER_1 };
#define HF_MSPM0_PWM_COUNT (sizeof(s_pwm_timer_table) / sizeof(s_pwm_timer_table[0]))

static const DL_TIMER_CC_INDEX s_cc_index_table[] = {
    DL_TIMER_CC_0_INDEX, DL_TIMER_CC_1_INDEX, DL_TIMER_CC_2_INDEX, DL_TIMER_CC_3_INDEX
};
#define HF_MSPM0_PWM_CHANNEL_COUNT (sizeof(s_cc_index_table) / sizeof(s_cc_index_table[0]))

static uint32_t s_period[HF_MSPM0_PWM_COUNT];

static GPTIMER_Regs *hf_decode_pwm_timer(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    if (instance >= HF_MSPM0_PWM_COUNT || s_pwm_timer_table[instance] == NULL) {
        return NULL;
    }
    return s_pwm_timer_table[instance];
}

static DL_TIMER_CC_INDEX hf_decode_pwm_cc(hf_pwm_t channel)
{
    return s_cc_index_table[channel & 0x3];
}

bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz)
{
    uint32_t instance = channel >> 2;
    GPTIMER_Regs *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL || freq_hz == 0U) {
        return false;
    }

    /* NOT independently confirmed — see file header comment. */
    DL_Timer_ClockConfig clockConfig = {0};
    clockConfig.clockSel = DL_TIMER_CLOCK_BUSCLK;
    clockConfig.divideRatio = DL_TIMER_CLOCK_DIVIDE_1;
    clockConfig.prescale = 0;
    DL_Timer_setClockConfig(timer, &clockConfig);

    uint32_t period = (1000000U / freq_hz); /* assumes 1 MHz post-divide tick rate */
    if (period > 0U) { period -= 1U; }
    s_period[instance] = period + 1U;

    DL_Timer_PWMConfig config = {0};
    config.period = period;
    config.pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN;
    config.isTimerWithFourCC = true;
    config.startTimer = DL_TIMER_STOP; /* NOT independently confirmed — see file header comment */
    DL_Timer_initFourCCPWMMode(timer, &config);

    DL_Timer_setCaptureCompareValue(timer, 0, hf_decode_pwm_cc(channel));
    return true;
}

bool hf_pwm_start(hf_pwm_t channel)
{
    GPTIMER_Regs *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL) {
        return false;
    }
    DL_Timer_startCounter(timer);
    return true;
}

void hf_pwm_stop(hf_pwm_t channel)
{
    GPTIMER_Regs *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL) {
        return;
    }
    DL_Timer_stopCounter(timer);
}

bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent)
{
    uint32_t instance = channel >> 2;
    GPTIMER_Regs *timer = hf_decode_pwm_timer(channel);
    if (timer == NULL || duty_millipercent > 100000U) {
        return false;
    }
    uint32_t compare = (uint32_t)(((uint64_t)duty_millipercent * s_period[instance]) / 100000U);
    DL_Timer_setCaptureCompareValue(timer, compare, hf_decode_pwm_cc(channel));
    return true;
}

#endif /* HF_ENABLE_PWM */
#endif /* TARGET_TI_MSPM0 */
