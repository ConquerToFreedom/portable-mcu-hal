/* STM32 HAL vendor adapter for the ADC portion of hal_wrapper.h.
 *
 * Same handle-ownership model as this vendor's I2C/UART/SPI adapters:
 * references CubeMX's own hadc1/hadc2/hadc3 externs (trim if your
 * project doesn't enable all three — same link-error-by-design
 * reasoning as those adapters).
 *
 * Resolution is advisory only, a different flavor of the same
 * limitation as this vendor's SPI/I2C clock_hz: hf_adc_init() does
 * NOT call HAL_ADC_Init() itself, since that would re-run CubeMX's
 * whole peripheral configuration (clock prescaler, conversion mode,
 * DMA continuous-request settings, etc.) using only whatever ST left
 * in hadc->Init from CubeMX-generated code — this file has no safe
 * way to reconstruct the rest of that struct. It assumes CubeMX's own
 * generated init already selected ADC_RESOLUTION_12B, the reset
 * default on most STM32 parts.
 *
 * Channel argument maps directly onto ADC_CHANNEL_0..ADC_CHANNEL_18 —
 * confirmed these are plain sequential integers (ADC_CHANNEL_1 =
 * ADC_CR1_AWDCH_0 = bit 0 = 1, ADC_CHANNEL_2 = ADC_CR1_AWDCH_1 = bit 1
 * = 2, and so on), not a bitmask needing translation — so
 * hf_adc_channel_t is passed straight through as sConfig.Channel, no
 * lookup table needed here the way this vendor's SPI cs_pin or I2C
 * device_addr might suggest.
 *
 * sConfig.Rank is written as the plain ordinal `1` here (this file's
 * reference is the F4 family's single-conversion regular sequencer).
 * Some newer STM32 families (F3/L4/G0/H7) use a named macro,
 * ADC_REGULAR_RANK_1, instead of the bare integer for the same slot —
 * substitute it if your part's HAL requires it.
 *
 * sConfig.SamplingTime is fixed at ADC_SAMPLETIME_3CYCLES, the
 * shortest this family exposes — edit it if your signal source has
 * high impedance and needs a longer acquisition window.
 *
 * Every call reconfigures the single regular-sequence channel via
 * HAL_ADC_ConfigChannel() before starting a conversion, since v1 only
 * ever runs one channel per hf_adc_read()/read_async() call — there's
 * no persistent multi-channel sequence programmed here.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_ADC

#include <stddef.h>
#include "main.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

static ADC_HandleTypeDef * const s_adc_handle_table[] = { &hadc1, &hadc2, &hadc3 };
#define HF_STM32_ADC_COUNT (sizeof(s_adc_handle_table) / sizeof(s_adc_handle_table[0]))

static uint16_t *s_value_ptr[HF_STM32_ADC_COUNT];
static volatile bool s_busy[HF_STM32_ADC_COUNT];
static hf_adc_callback_t s_callback[HF_STM32_ADC_COUNT];

static ADC_HandleTypeDef *hf_decode_adc(hf_adc_t instance)
{
    if (instance >= HF_STM32_ADC_COUNT) {
        return NULL;
    }
    return s_adc_handle_table[instance];
}

static int hf_adc_index_of(ADC_HandleTypeDef *hadc)
{
    size_t i;
    for (i = 0; i < HF_STM32_ADC_COUNT; i++) {
        if (s_adc_handle_table[i] == hadc) {
            return (int)i;
        }
    }
    return -1;
}

static bool hf_adc_config_channel(ADC_HandleTypeDef *hadc, hf_adc_channel_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1; /* F4-style plain ordinal — see file header comment */
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    return HAL_ADC_ConfigChannel(hadc, &sConfig) == HAL_OK;
}

bool hf_adc_init(hf_adc_t instance)
{
    return hf_decode_adc(instance) != NULL;
}

bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC_HandleTypeDef *hadc = hf_decode_adc(instance);
    if (hadc == NULL || !hf_adc_config_channel(hadc, channel)) {
        return false;
    }
    if (HAL_ADC_Start(hadc) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY) != HAL_OK) {
        HAL_ADC_Stop(hadc);
        return false;
    }
    *value = (uint16_t)HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);
    return true;
}

void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback)
{
    if (instance < HF_STM32_ADC_COUNT) {
        s_callback[instance] = callback;
    }
}

bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC_HandleTypeDef *hadc = hf_decode_adc(instance);
    if (hadc == NULL || s_busy[instance] || !hf_adc_config_channel(hadc, channel)) {
        return false;
    }
    s_value_ptr[instance] = value;
    s_busy[instance] = true;
    if (HAL_ADC_Start_IT(hadc) != HAL_OK) {
        s_busy[instance] = false;
        return false;
    }
    return true;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    int index = hf_adc_index_of(hadc);
    if (index < 0) {
        return;
    }
    *s_value_ptr[index] = (uint16_t)HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop_IT(hadc);
    s_busy[index] = false;
    if (s_callback[index] != NULL) {
        s_callback[index]((hf_adc_t)index);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    int index = hf_adc_index_of(hadc);
    if (index < 0) {
        return;
    }
    HAL_ADC_Stop_IT(hadc);
    /* hf_adc_callback_t carries no success/failure signal (same
     * reasoning as UART/SPI's callbacks) — clear busy without
     * invoking the callback, since firing it would incorrectly imply
     * success. */
    s_busy[index] = false;
}

#endif /* HF_ENABLE_ADC */
#endif /* TARGET_STM32 */
