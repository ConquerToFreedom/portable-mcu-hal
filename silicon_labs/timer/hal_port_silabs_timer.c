/* Silicon Labs (emlib) vendor adapter for the Timer portion of
 * hal_wrapper.h — the classic/legacy API, gated by TARGET_SILABS.
 * Verified TIMER_Init_TypeDef's fields and TIMER_Init/TIMER_TopSet/
 * TIMER_Enable's signatures directly against
 * SiliconLabs/Gecko_SDK's platform/emlib/inc/em_timer.h (gsdk_4.5)
 * before writing this.
 *
 * Genuine hardware feature, not software-enforced: TIMER_Init_TypeDef
 * has its own `oneShot` field — the counter stops itself in hardware
 * after the next overflow when set, rather than this adapter having to
 * intervene from the interrupt handler the way STM32's basic-timer
 * adapter does. Real, useful vendor asymmetry, not a coincidence this
 * project should hide.
 *
 * `period_us` is advisory in the same sense as this vendor's other
 * clock-rate-shaped parameters: turning it into an accurate TOP value
 * assumes a 1 MHz HFPERCLK-after-prescale tick rate (prescale left at
 * `timerPrescale1`, i.e. no additional division), which this file
 * cannot verify — the real HFPERCLK frequency depends on the part and
 * the customer's own clock tree configuration (commonly in the
 * 19-40 MHz range before any prescaling). If your part's HFPERCLK
 * isn't already 1 MHz, `period_us` will be off by that ratio — EDIT the
 * `prescale` field below to match your part's actual clock, same "EDIT
 * ME, board/part-specific" treatment this vendor's I2C SCL/SDA pin
 * table and IADC channel table already get.
 *
 * TIMER_IF_OF/TIMER_IEN_OF (the overflow-interrupt flag, which this
 * adapter uses as the "period elapsed" event) follow emlib's
 * well-established IF_<event>/IEN_<event> naming convention (already
 * relied on for this vendor's USART_IF_TXBL/ADC_IF_SINGLE elsewhere in
 * this repo) but weren't independently re-confirmed by a direct quote
 * from source in this fetch pass — flagged the same way this vendor's
 * other files flag their own unconfirmed specifics.
 *
 * Same clock-enable bug this vendor's GPIO adapter had: TIMER_Init()
 * does not enable the peripheral's own bus clock — hf_timer_init() now
 * enables the decoded instance's TIMER clock unconditionally before
 * TIMER_Init(), idempotent same as the GPIO fix.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_TIMER

#include "em_timer.h"
#include "em_cmu.h"

#ifdef TIMER0
#define HF_SILABS_TIMER_0 TIMER0
#else
#define HF_SILABS_TIMER_0 NULL
#endif
#ifdef TIMER1
#define HF_SILABS_TIMER_1 TIMER1
#else
#define HF_SILABS_TIMER_1 NULL
#endif

static TIMER_TypeDef * const s_timer_table[] = { HF_SILABS_TIMER_0, HF_SILABS_TIMER_1 };
#define HF_SILABS_TIMER_COUNT (sizeof(s_timer_table) / sizeof(s_timer_table[0]))

#define TIMER_IF_OF  (1U << 0)
#define TIMER_IEN_OF TIMER_IF_OF

static hf_timer_callback_t s_callback[HF_SILABS_TIMER_COUNT];

static TIMER_TypeDef *hf_decode_timer(hf_timer_t instance)
{
    if (instance >= HF_SILABS_TIMER_COUNT || s_timer_table[instance] == NULL) {
        return NULL;
    }
    return s_timer_table[instance];
}

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode)
{
    TIMER_TypeDef *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }

#ifdef TIMER0
    if (timer == TIMER0) { CMU_ClockEnable(cmuClock_TIMER0, true); }
#endif
#ifdef TIMER1
    if (timer == TIMER1) { CMU_ClockEnable(cmuClock_TIMER1, true); }
#endif

    TIMER_Init_TypeDef init = TIMER_INIT_DEFAULT;
    init.enable = false; /* configure only — hf_timer_start() enables counting */
    init.prescale = timerPrescale1; /* assumes 1 MHz HFPERCLK — see file header comment, EDIT ME otherwise */
    init.oneShot = (mode == HF_TIMER_ONE_SHOT);

    TIMER_IntClear(timer, TIMER_IF_OF);
    TIMER_IntEnable(timer, TIMER_IEN_OF);

#ifdef TIMER0
    if (timer == TIMER0) {
        NVIC_EnableIRQ(TIMER0_IRQn);
    }
#endif
#ifdef TIMER1
    if (timer == TIMER1) {
        NVIC_EnableIRQ(TIMER1_IRQn);
    }
#endif

    TIMER_TopSet(timer, (period_us > 0U) ? (period_us - 1U) : 0U);
    TIMER_Init(timer, &init);
    return true;
}

bool hf_timer_start(hf_timer_t instance)
{
    TIMER_TypeDef *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }
    TIMER_CounterSet(timer, 0);
    TIMER_Enable(timer, true);
    return true;
}

void hf_timer_stop(hf_timer_t instance)
{
    TIMER_TypeDef *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return;
    }
    TIMER_Enable(timer, false);
}

void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback)
{
    if (instance < HF_SILABS_TIMER_COUNT) {
        s_callback[instance] = callback;
    }
}

static void hf_timer_service_irq(hf_timer_t instance, TIMER_TypeDef *timer)
{
    if (TIMER_IntGet(timer) & TIMER_IF_OF) {
        TIMER_IntClear(timer, TIMER_IF_OF);
        if (s_callback[instance] != NULL) {
            s_callback[instance](instance);
        }
    }
}

#ifdef TIMER0
void TIMER0_IRQHandler(void)
{
    hf_timer_service_irq(0, TIMER0);
}
#endif
#ifdef TIMER1
void TIMER1_IRQHandler(void)
{
    hf_timer_service_irq(1, TIMER1);
}
#endif

#endif /* HF_ENABLE_TIMER */
#endif /* TARGET_SILABS */
