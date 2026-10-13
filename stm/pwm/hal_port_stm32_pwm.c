/* STM32 HAL vendor adapter for the PWM portion of hal_wrapper.h.
 *
 * Same handle-ownership model as this vendor's other adapters:
 * references CubeMX's own htim2/htim3 externs rather than owning
 * private handles (trim if your project doesn't enable both — same
 * link-error-by-design reasoning as the I2C/UART/SPI/ADC adapters).
 *
 * Deliberately TIM2/TIM3, not TIM6/TIM7 (which hf_timer_t already
 * claims): STM32's basic timers have no output-compare channels or
 * pins at all, so PWM is unconditionally forced onto a general-purpose
 * timer regardless of instance numbering on either side — see
 * hf_pwm_t's own header comment in hal_wrapper.h. TIM2/TIM3 are
 * 4-channel general-purpose timers on every STM32 family that has
 * them; substitute a different general-purpose timer if your part
 * lacks one of these, same "trim the table to match your part"
 * precedent as this vendor's other adapters.
 *
 * hf_pwm_t's (instance << 2) | channel encoding maps channel 0-3 onto
 * TIM_CHANNEL_1..TIM_CHANNEL_4 via a lookup table — these are NOT
 * 0/1/2/3 on real hardware (TIM_CHANNEL_2 = 0x4, TIM_CHANNEL_3 = 0x8,
 * TIM_CHANNEL_4 = 0xC), so the table, not arithmetic, does the
 * translation.
 *
 * `freq_hz` is advisory in the same sense as this vendor's other
 * clock-rate-shaped parameters (UART's baud rate is the one exception
 * that's genuinely exact): turning it into an exact ARR value assumes
 * CubeMX's generated MX_TIMx_Init() already configured this timer's
 * prescaler so one counter tick equals one microsecond (i.e. a 1 MHz
 * post-prescale tick rate) — the same assumption hf_timer_t's STM32
 * adapter already documents and makes for TIM6/TIM7. If your project's
 * CubeMX setup uses a different tick rate, the resulting frequency
 * will be off by that ratio.
 *
 * HAL_TIM_PWM_ConfigChannel()'s signature — (htim, sConfig, Channel) —
 * wasn't re-quotable from a fetched source in this pass (the vendor's
 * stm32f4xx_hal_tim.c is large enough that the fetch tool truncated
 * before reaching it), but is confirmed via HAL_TIM_PWM_Init()'s own
 * signature plus this family's extremely stable, uniform
 * HAL_TIM_*_ConfigChannel(htim, config, Channel) calling convention
 * (shared by HAL_TIM_OC_ConfigChannel, HAL_TIM_IC_ConfigChannel, and
 * every other channel-shaped HAL_TIM config function) — not
 * independently re-verified byte-for-byte the way most of this
 * project's other signatures are, flagged the same honest way as
 * HAL_TIM_PeriodElapsedCallback's weak declaration in hf_timer_t's own
 * adapter.
 *
 * Runtime duty updates use __HAL_TIM_SET_COMPARE(), not a second
 * HAL_TIM_PWM_ConfigChannel() call — confirmed the vendor's own header
 * comment states this macro "set[s] the TIM Capture Compare Register
 * value on runtime without calling another time ConfigChannel
 * function", which is exactly the "change duty without a glitch or
 * re-init" behavior hf_pwm_set_duty()'s contract promises.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_PWM

#include <stddef.h>
#include "main.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

static TIM_HandleTypeDef * const s_pwm_handle_table[] = { &htim2, &htim3 };
#define HF_STM32_PWM_COUNT (sizeof(s_pwm_handle_table) / sizeof(s_pwm_handle_table[0]))

static const uint32_t s_tim_channel_table[] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};
#define HF_STM32_PWM_CHANNEL_COUNT (sizeof(s_tim_channel_table) / sizeof(s_tim_channel_table[0]))

static uint32_t s_period[HF_STM32_PWM_COUNT];

static TIM_HandleTypeDef *hf_decode_pwm_timer(hf_pwm_t channel)
{
    uint32_t instance = channel >> 2;
    if (instance >= HF_STM32_PWM_COUNT) {
        return NULL;
    }
    return s_pwm_handle_table[instance];
}

static uint32_t hf_decode_pwm_channel(hf_pwm_t channel)
{
    return s_tim_channel_table[channel & 0x3];
}

bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz)
{
    uint32_t instance = channel >> 2;
    TIM_HandleTypeDef *htim = hf_decode_pwm_timer(channel);
    if (htim == NULL || freq_hz == 0U) {
        return false;
    }
    uint32_t tim_channel = hf_decode_pwm_channel(channel);

    /* Assumes a 1 MHz post-prescale tick, same advisory-only reasoning
     * as hf_timer_t's STM32 adapter — see file header comment. */
    uint32_t period = (1000000U / freq_hz);
    htim->Init.Period = (period > 0U) ? (period - 1U) : 0U;
    s_period[instance] = htim->Init.Period + 1U;

    if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        return false;
    }

    TIM_OC_InitTypeDef sConfig = {0};
    sConfig.OCMode = TIM_OCMODE_PWM1;
    sConfig.Pulse = 0;
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode = TIM_OCFAST_DISABLE;

    return HAL_TIM_PWM_ConfigChannel(htim, &sConfig, tim_channel) == HAL_OK;
}

bool hf_pwm_start(hf_pwm_t channel)
{
    TIM_HandleTypeDef *htim = hf_decode_pwm_timer(channel);
    if (htim == NULL) {
        return false;
    }
    return HAL_TIM_PWM_Start(htim, hf_decode_pwm_channel(channel)) == HAL_OK;
}

void hf_pwm_stop(hf_pwm_t channel)
{
    TIM_HandleTypeDef *htim = hf_decode_pwm_timer(channel);
    if (htim == NULL) {
        return;
    }
    (void)HAL_TIM_PWM_Stop(htim, hf_decode_pwm_channel(channel));
}

bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent)
{
    uint32_t instance = channel >> 2;
    TIM_HandleTypeDef *htim = hf_decode_pwm_timer(channel);
    if (htim == NULL || duty_millipercent > 100000U) {
        return false;
    }
    uint32_t compare = (uint32_t)(((uint64_t)duty_millipercent * s_period[instance]) / 100000U);
    __HAL_TIM_SET_COMPARE(htim, hf_decode_pwm_channel(channel), compare);
    return true;
}

#endif /* HF_ENABLE_PWM */
#endif /* TARGET_STM32 */
