/* Silicon Labs adapter for the Timer portion of hal_wrapper.h, against
 * the current `sl_sleeptimer` service — gated by TARGET_SILABS_SLGPIO,
 * the same target macro the current sl_gpio/sl_i2c/IADC adapters use.
 * Verified sl_sleeptimer_timer_handle_t, sl_sleeptimer_start_timer/
 * start_periodic_timer/stop_timer, and the ms-to-tick conversion
 * functions directly against
 * SiliconLabs/simplicity_sdk's platform/service/sleeptimer/inc/
 * sl_sleeptimer.h (sisdk-2025.6) before writing this.
 *
 * GENUINELY DIFFERENT STRUCTURE from every other adapter in this repo,
 * not just a different vendor API: sl_sleeptimer is a software timer
 * *service*, not a "pick the vendor's Nth hardware peripheral" driver.
 * One shared hardware timer underneath (typically RTCC or BURTC,
 * config-dependent) backs an effectively unlimited number of
 * independent software timer handles — the opposite situation from,
 * say, nRF52's SAADC ("only one instance exists"). This adapter still
 * exposes hf_timer_t as a small fixed-size table (matching this
 * project's other vendors' instance-count conventions for API
 * consistency across TARGET_SILABS and TARGET_SILABS_SLGPIO), but
 * unlike every other instance table in this repo, this one is a purely
 * software-imposed limit, not a hardware one — raise
 * HF_SILABS_SLEEPTIMER_COUNT freely if you need more concurrent
 * timers.
 *
 * REAL RESOLUTION FINDING: sl_sleeptimer.h exposes only millisecond-
 * granularity time conversion (sl_sleeptimer_ms_to_tick/ms32_to_tick) —
 * no microsecond variant exists in this API, unlike every other vendor
 * this project's hf_timer_t targets. `period_us` is therefore rounded
 * UP to whole milliseconds here (a period under 1000us still gets at
 * least 1ms), a genuine, permanent resolution loss on this vendor
 * specifically, not an oversight — document it, don't hide it, same
 * treatment as every other vendor asymmetry in this project.
 *
 * Same class of bug this vendor's GPIO adapter had, different shape:
 * sl_sleeptimer requires a one-time sl_sleeptimer_init() call before
 * ANY of its timer functions work, including sl_sleeptimer_ms32_to_tick()
 * (it needs the underlying hardware timer's real running frequency,
 * which isn't known until init runs) — confirmed against
 * platform/service/sleeptimer/src/sl_sleeptimer.c in SiliconLabs/
 * simplicity_sdk. This adapter never called it. hf_timer_init() now
 * calls sl_sleeptimer_init() unconditionally first; it's guarded
 * internally by its own "already initialized" flag, so — like
 * sl_gpio_init() in the GPIO adapter — it's safe to call on every
 * hf_timer_init() with no guard of our own.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_TIMER

#include "sl_sleeptimer.h"

#define HF_SILABS_SLEEPTIMER_COUNT 2

static sl_sleeptimer_timer_handle_t s_handles[HF_SILABS_SLEEPTIMER_COUNT];
static hf_timer_mode_t s_mode[HF_SILABS_SLEEPTIMER_COUNT];
static uint32_t s_ticks[HF_SILABS_SLEEPTIMER_COUNT];
static hf_timer_callback_t s_callback[HF_SILABS_SLEEPTIMER_COUNT];

static void hf_timer_trampoline(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    hf_timer_t instance = (hf_timer_t)(uintptr_t)data;
    if (instance < HF_SILABS_SLEEPTIMER_COUNT && s_callback[instance] != NULL) {
        s_callback[instance](instance);
    }
}

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode)
{
    if (instance >= HF_SILABS_SLEEPTIMER_COUNT) {
        return false;
    }

    (void)sl_sleeptimer_init();

    /* Rounded UP to whole milliseconds — see file header comment. */
    uint32_t period_ms = (period_us + 999U) / 1000U;
    if (period_ms == 0U) {
        period_ms = 1U;
    }

    if (sl_sleeptimer_ms32_to_tick(period_ms, &s_ticks[instance]) != SL_STATUS_OK) {
        return false;
    }
    s_mode[instance] = mode;
    return true;
}

bool hf_timer_start(hf_timer_t instance)
{
    if (instance >= HF_SILABS_SLEEPTIMER_COUNT) {
        return false;
    }

    sl_status_t status;
    void *context = (void *)(uintptr_t)instance;
    if (s_mode[instance] == HF_TIMER_PERIODIC) {
        status = sl_sleeptimer_start_periodic_timer(&s_handles[instance], s_ticks[instance],
                                                     hf_timer_trampoline, context, 0, 0);
    } else {
        status = sl_sleeptimer_start_timer(&s_handles[instance], s_ticks[instance],
                                            hf_timer_trampoline, context, 0, 0);
    }
    return status == SL_STATUS_OK;
}

void hf_timer_stop(hf_timer_t instance)
{
    if (instance >= HF_SILABS_SLEEPTIMER_COUNT) {
        return;
    }
    (void)sl_sleeptimer_stop_timer(&s_handles[instance]);
}

void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback)
{
    if (instance < HF_SILABS_SLEEPTIMER_COUNT) {
        s_callback[instance] = callback;
    }
}

#endif /* HF_ENABLE_TIMER */
#endif /* TARGET_SILABS_SLGPIO */
