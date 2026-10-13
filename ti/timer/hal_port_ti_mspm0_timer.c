/* TI MSPM0 (driverlib) vendor adapter for the Timer portion of
 * hal_wrapper.h, against DL_TimerG (general-purpose timer — TimerA is
 * MSPM0's advanced/capture-compare-and-PWM-capable family, reserved
 * for the not-yet-designed PWM peripheral so the two don't compete for
 * the same hardware by default). Assumes the customer's SysConfig-
 * generated "ti_msp_dl_config.h" is on the include path, same
 * assumption as this vendor's other adapters.
 *
 * Verified DL_Timer_ClockConfig/DL_Timer_TimerConfig's field lists,
 * DL_Timer_initTimerMode/setClockConfig/startCounter/stopCounter's
 * signatures, and the DL_TIMER_TIMER_MODE_ONE_SHOT/_PERIODIC (both
 * explicitly "in DOWN COUNTING") and DL_TIMER_IIDX_ZERO/_LOAD
 * interrupt-index names directly against
 * TexasInstruments/mspm0-sdk's own source/ti/driverlib/dl_timer.h and
 * dl_timerg.h on GitHub (same higher-confidence source this vendor's
 * SPI/ADC adapters used) before writing this — confirming
 * DL_TIMER_TIMER_MODE_ONE_SHOT is a genuine hardware feature here too,
 * same as classic Silicon Labs' `oneShot` field: no software
 * enforcement needed, the down-counter simply doesn't reload after
 * reaching zero once in this mode.
 *
 * Several specifics below were NOT independently confirmed in this
 * fetch pass and are flagged individually rather than presented with
 * false confidence — cross-check all of them against your installed
 * MSPM0-SDK version's dl_timer.h before relying on this in production:
 *   1. DL_TIMER_CLOCK_BUSCLK — the clock source selector passed to
 *      DL_Timer_ClockConfig.clockSel. This file assumes it exists and
 *      selects the bus clock; the actual enum's full member list
 *      wasn't reached in this fetch.
 *   2. DL_Timer_TimerConfig.startTimer — this file passes a value
 *      assumed to mean "don't auto-start" (matching this project's
 *      init-configures-without-starting contract), inferred from the
 *      field's name and purpose, not a directly confirmed enum value.
 *   3. DL_Timer_TimerConfig.genIntermInt — set to a value assumed to
 *      mean "disabled"; not independently confirmed.
 *   4. DL_TIMER_INTERRUPT_ZERO — the "enable interrupt" counterpart to
 *      the confirmed DL_TIMER_IIDX_ZERO status/index constant, assumed
 *      by the same enable/disable-and-status-flag naming pairing
 *      convention already flagged for DL_SPI_disableInterrupt and
 *      DL_ADC12_disableInterrupt in this vendor's other adapters.
 *   5. TIMG0_IRQHandler/TIMG1_IRQHandler — the IRQ vector names,
 *      following this vendor's own established per-peripheral pattern
 *      (UART0_IRQHandler, I2C0_IRQHandler, SPI0_IRQHandler,
 *      ADC0_IRQHandler all confirmed elsewhere in this repo) but not
 *      confirmed directly for TimerG in this fetch pass.
 *
 * `period_us` is advisory in the same sense as this vendor's other
 * clock-rate-shaped parameters (UART's baud_rate, I2C/SPI's clock_hz):
 * turning it into an accurate load value assumes the bus clock (after
 * DL_Timer_ClockConfig.divideRatio/prescale) yields a 1 MHz tick rate,
 * which this file cannot verify — SysConfig's own clock-tree
 * configuration decides the real answer. EDIT the clockConfig fields
 * below if your project's bus clock isn't already 1 MHz post-divide.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_TIMER

#include "ti_msp_dl_config.h"

#ifdef TIMG0
#define HF_MSPM0_TIMER_0 TIMG0
#else
#define HF_MSPM0_TIMER_0 NULL
#endif
#ifdef TIMG1
#define HF_MSPM0_TIMER_1 TIMG1
#else
#define HF_MSPM0_TIMER_1 NULL
#endif

static GPTIMER_Regs * const s_timer_table[] = { HF_MSPM0_TIMER_0, HF_MSPM0_TIMER_1 };
#define HF_MSPM0_TIMER_COUNT (sizeof(s_timer_table) / sizeof(s_timer_table[0]))

/* NOT independently confirmed — see file header comment. */
#define HF_MSPM0_TIMER_INTERRUPT_ZERO DL_TIMER_INTERRUPT_ZERO

static hf_timer_callback_t s_callback[HF_MSPM0_TIMER_COUNT];

static GPTIMER_Regs *hf_decode_timer(hf_timer_t instance)
{
    if (instance >= HF_MSPM0_TIMER_COUNT || s_timer_table[instance] == NULL) {
        return NULL;
    }
    return s_timer_table[instance];
}

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode)
{
    GPTIMER_Regs *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }

    /* NOT independently confirmed — see file header comment. */
    DL_Timer_ClockConfig clockConfig = {0};
    clockConfig.clockSel = DL_TIMER_CLOCK_BUSCLK;
    clockConfig.divideRatio = DL_TIMER_CLOCK_DIVIDE_1;
    clockConfig.prescale = 0;
    DL_Timer_setClockConfig(timer, &clockConfig);

    DL_Timer_TimerConfig config = {0};
    config.period = (period_us > 0U) ? (period_us - 1U) : 0U; /* assumes 1 MHz post-divide tick rate */
    config.timerMode = (mode == HF_TIMER_PERIODIC)
        ? DL_TIMER_TIMER_MODE_PERIODIC
        : DL_TIMER_TIMER_MODE_ONE_SHOT;
    config.startTimer = DL_TIMER_STOP; /* NOT independently confirmed — see file header comment */
    config.genIntermInt = DL_TIMER_INTERM_INT_DISABLED; /* NOT independently confirmed */
    config.counterVal = 0;
    DL_Timer_initTimerMode(timer, &config);

    DL_Timer_enableInterrupt(timer, HF_MSPM0_TIMER_INTERRUPT_ZERO);
    return true;
}

bool hf_timer_start(hf_timer_t instance)
{
    GPTIMER_Regs *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }
    DL_Timer_startCounter(timer);
    return true;
}

void hf_timer_stop(hf_timer_t instance)
{
    GPTIMER_Regs *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return;
    }
    DL_Timer_stopCounter(timer);
}

void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback)
{
    if (instance < HF_MSPM0_TIMER_COUNT) {
        s_callback[instance] = callback;
    }
}

static void hf_timer_service_irq(hf_timer_t instance, GPTIMER_Regs *timer)
{
    if (DL_Timer_getPendingInterrupt(timer) == DL_TIMER_IIDX_ZERO) {
        if (s_callback[instance] != NULL) {
            s_callback[instance](instance);
        }
    }
}

#ifdef TIMG0
void TIMG0_IRQHandler(void)
{
    hf_timer_service_irq(0, TIMG0);
}
#endif
#ifdef TIMG1
void TIMG1_IRQHandler(void)
{
    hf_timer_service_irq(1, TIMG1);
}
#endif

#endif /* HF_ENABLE_TIMER */
#endif /* TARGET_TI_MSPM0 */
