/* STM32 HAL vendor adapter for the Timer portion of hal_wrapper.h.
 *
 * Same handle-ownership model as this vendor's other adapters:
 * references CubeMX's own htim6/htim7 externs rather than owning
 * private handles (trim if your project doesn't enable both — same
 * link-error-by-design reasoning as the I2C/UART/SPI/ADC adapters).
 * TIM6/TIM7 are STM32's "basic timers" specifically: no I/O pins, no
 * capture/compare channels, existing purely to count and fire an
 * update interrupt — the correct hardware for a headless periodic
 * timer, the same way this project picked basic ADC/USART blocks for
 * other peripherals rather than something with more capability than
 * needed. Not every STM32 part has both TIM6 and TIM7 (or either) —
 * substitute a general-purpose timer (e.g. TIM2) used in counter-only
 * mode if your part lacks basic timers, same "trim the table to match
 * your part" precedent as this vendor's other adapters.
 *
 * `period_us` is advisory in the sense that turning it into an exact
 * ARR (auto-reload) value assumes CubeMX's generated MX_TIMx_Init()
 * already configured this timer's prescaler so that one counter tick
 * equals one microsecond (i.e. the timer's input clock, after
 * prescaling, is 1 MHz) — a common, deliberate CubeMX configuration
 * choice for exactly this use case, but not something this file can
 * verify or set up itself (the actual APB timer clock varies by part
 * and by the customer's own clock tree configuration). If your
 * project's CubeMX setup uses a different tick rate, `period_us` will
 * be off by that ratio — same category of limitation as this vendor's
 * SPI/I2C `clock_hz` and UART's advisory-only paths.
 *
 * One-shot mode is software-enforced, not STM32's own hardware
 * One-Pulse Mode (OPM): OPM is fundamentally an output-channel/pulse-
 * generation feature (HAL_TIM_OnePulse_Init needs an output channel
 * configuration and is normally driven by an input trigger), not a
 * fit for this headless, channel-less periodic-timer contract. Instead,
 * HAL_TIM_PeriodElapsedCallback() below calls HAL_TIM_Base_Stop_IT()
 * itself immediately after invoking the registered callback whenever
 * the timer was configured HF_TIMER_ONE_SHOT — simple, correct, and
 * consistent with every other "software tracks what hardware doesn't
 * give us for free" choice already made elsewhere in this project.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_TIMER

#include <stddef.h>
#include "main.h"

extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

static TIM_HandleTypeDef * const s_timer_handle_table[] = { &htim6, &htim7 };
#define HF_STM32_TIMER_COUNT (sizeof(s_timer_handle_table) / sizeof(s_timer_handle_table[0]))

static hf_timer_mode_t s_mode[HF_STM32_TIMER_COUNT];
static hf_timer_callback_t s_callback[HF_STM32_TIMER_COUNT];

static TIM_HandleTypeDef *hf_decode_timer(hf_timer_t instance)
{
    if (instance >= HF_STM32_TIMER_COUNT) {
        return NULL;
    }
    return s_timer_handle_table[instance];
}

static int hf_timer_index_of(TIM_HandleTypeDef *htim)
{
    size_t i;
    for (i = 0; i < HF_STM32_TIMER_COUNT; i++) {
        if (s_timer_handle_table[i] == htim) {
            return (int)i;
        }
    }
    return -1;
}

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode)
{
    TIM_HandleTypeDef *htim = hf_decode_timer(instance);
    if (htim == NULL) {
        return false;
    }

    /* Assumes a 1 MHz post-prescaler tick rate — see file header
     * comment. ARR is zero-indexed (counts 0..Period then overflows),
     * so N ticks of period needs Period = N - 1. */
    htim->Init.Period = (period_us > 0) ? (period_us - 1U) : 0U;

    if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return false;
    }

    s_mode[instance] = mode;
    return true;
}

bool hf_timer_start(hf_timer_t instance)
{
    TIM_HandleTypeDef *htim = hf_decode_timer(instance);
    if (htim == NULL) {
        return false;
    }
    return HAL_TIM_Base_Start_IT(htim) == HAL_OK;
}

void hf_timer_stop(hf_timer_t instance)
{
    TIM_HandleTypeDef *htim = hf_decode_timer(instance);
    if (htim == NULL) {
        return;
    }
    (void)HAL_TIM_Base_Stop_IT(htim);
}

void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback)
{
    if (instance < HF_STM32_TIMER_COUNT) {
        s_callback[instance] = callback;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    int index = hf_timer_index_of(htim);
    if (index < 0) {
        return;
    }
    if (s_callback[index] != NULL) {
        s_callback[index]((hf_timer_t)index);
    }
    if (s_mode[index] == HF_TIMER_ONE_SHOT) {
        /* Hardware has no built-in "stop after one period" for a
         * headless basic timer (see file header comment) — enforce it
         * here so the caller's contract ("fires once, then stays
         * stopped until hf_timer_start() is called again") holds. */
        (void)HAL_TIM_Base_Stop_IT(htim);
    }
}

#endif /* HF_ENABLE_TIMER */
#endif /* TARGET_STM32 */
