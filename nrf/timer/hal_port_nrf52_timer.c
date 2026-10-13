/* nRF52 (nrfx) vendor adapter for the Timer portion of hal_wrapper.h,
 * against nrfx_timer (general-purpose 32-bit timer/counter). Verified
 * nrfx_timer.h and nrf_timer.h directly against
 * NordicSemiconductor/nrfx before writing this.
 *
 * Uses TIMER1/TIMER2, not TIMER0 — TIMER0 is conventionally reserved
 * for the SoftDevice/radio stack on many nRF52 projects (a real,
 * deliberate choice, not an arbitrary starting index the way this
 * project's other "instance 0, 1, 2..." tables are).
 *
 * Configured for a 1 MHz counting frequency and 32-bit width
 * (NRF_TIMER_BIT_WIDTH_32 — at 1 MHz a 16-bit counter would overflow
 * after ~65ms, too short for a general-purpose periodic timer), so
 * `period_us` converts to a CC (compare) value via nrfx_timer_us_to_
 * ticks() directly, unlike this vendor's other adapters' advisory-only
 * clock_hz/baud_rate parameters — genuinely accurate here since this
 * project owns the timer's clock configuration outright, with no
 * CubeMX/SysConfig-style external pre-configuration step to defer to
 * or guess at the way STM32/TI's timer adapters must.
 *
 * One-shot vs. periodic is a genuine hardware feature here, not
 * software-enforced: nrfx_timer_extended_compare()'s shortcut mask
 * wires the compare event directly to a TASKS_STOP (one-shot) or
 * TASKS_CLEAR (periodic — the counter restarts from 0 in hardware,
 * immediately counting toward the same compare value again) via the
 * PPI-like internal shortcut mechanism, entirely without CPU
 * intervention between periods.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_TIMER

#include <stdint.h>
#include "nrfx_timer.h"

#define HF_NRF52_TIMER_IRQ_PRIORITY 6
#define HF_NRF52_TIMER_FREQUENCY_HZ 1000000UL /* 1 MHz -> 1 tick == 1 microsecond */
#define HF_NRF52_TIMER_CC_CHANNEL   0

static nrfx_timer_t s_timer_instances[] = {
    NRFX_TIMER_INSTANCE(1),
    NRFX_TIMER_INSTANCE(2),
};
#define HF_NRF52_TIMER_COUNT (sizeof(s_timer_instances) / sizeof(s_timer_instances[0]))

static hf_timer_callback_t s_callback[HF_NRF52_TIMER_COUNT];

static nrfx_timer_t *hf_decode_timer(hf_timer_t instance)
{
    if (instance >= HF_NRF52_TIMER_COUNT) {
        return NULL;
    }
    return &s_timer_instances[instance];
}

static void hf_timer_event_handler(nrf_timer_event_t event_type, void *p_context)
{
    hf_timer_t instance = (hf_timer_t)(uintptr_t)p_context;

    if (event_type != NRF_TIMER_EVENT_COMPARE0) {
        return;
    }
    if (instance < HF_NRF52_TIMER_COUNT && s_callback[instance] != NULL) {
        s_callback[instance](instance);
    }
}

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode)
{
    nrfx_timer_t *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }

    nrfx_timer_config_t config = {0};
    config.frequency = HF_NRF52_TIMER_FREQUENCY_HZ;
    config.mode = NRF_TIMER_MODE_TIMER;
    config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    config.interrupt_priority = HF_NRF52_TIMER_IRQ_PRIORITY;
    /* Delivered back to hf_timer_event_handler() as p_context, unlike
     * this vendor's UART adapter's nrfx_uarte_init() (see that file's
     * own flagged finding) — nrfx_timer_config_t carries p_context
     * itself, so there is no equivalent ambiguity here. */
    config.p_context = (void *)(uintptr_t)instance;

    if (nrfx_timer_init(timer, &config, hf_timer_event_handler) != NRFX_SUCCESS) {
        return false;
    }

    uint32_t ticks = nrfx_timer_us_to_ticks(timer, period_us);
    nrf_timer_short_mask_t short_mask = (mode == HF_TIMER_PERIODIC)
        ? NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK
        : NRF_TIMER_SHORT_COMPARE0_STOP_MASK;
    nrfx_timer_extended_compare(timer, HF_NRF52_TIMER_CC_CHANNEL, ticks, short_mask, true);
    return true;
}

bool hf_timer_start(hf_timer_t instance)
{
    nrfx_timer_t *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return false;
    }
    nrfx_timer_clear(timer);
    nrfx_timer_enable(timer);
    return true;
}

void hf_timer_stop(hf_timer_t instance)
{
    nrfx_timer_t *timer = hf_decode_timer(instance);
    if (timer == NULL) {
        return;
    }
    nrfx_timer_disable(timer);
}

void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback)
{
    if (instance < HF_NRF52_TIMER_COUNT) {
        s_callback[instance] = callback;
    }
}

#endif /* HF_ENABLE_TIMER */
#endif /* TARGET_NRF52 */
