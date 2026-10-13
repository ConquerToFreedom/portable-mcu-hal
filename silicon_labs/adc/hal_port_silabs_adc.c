/* Silicon Labs (emlib) vendor adapter for the ADC portion of
 * hal_wrapper.h — the classic/legacy API, gated by TARGET_SILABS.
 *
 * Genuinely different situation from this vendor's UART/SPI adapters,
 * where no independent "current" driver exists and both target macros
 * end up sharing one implementation: ADC's current-generation
 * replacement, IADC (Incremental ADC — see
 * hal_port_silabs_slgpio_adc.c), is a genuinely different hardware
 * peripheral block, not just a newer driver layer over the same
 * registers. Many Series 2 parts have IADC only, no classic ADC
 * hardware at all — the same "two real, incompatible adapters"
 * situation this vendor's GPIO and I2C adapters already document, not
 * UART/SPI's "one shared driver, two target macros" situation.
 *
 * Channel selection is a genuine wrinkle on this vendor specifically:
 * unlike STM32/nRF52/TI, where a channel argument passes straight
 * through as (or maps 1:1 onto) the vendor's own plain channel
 * number, classic EFM32/EFR32 ADC exposes TWO different
 * channel-selection struct fields depending on the part family:
 * `input` (ADC_SingleInput_TypeDef, simple adcSingleInputCh0..Ch7
 * constants) on non-APORT parts, or `posSel` (ADC_PosSel_TypeDef,
 * APORT-crossbar-routed adcPosSelAPORT... constants) on APORT parts.
 * This file uses the simpler `input`/adcSingleInputCh0..Ch7 field —
 * correct for Series 0/1 non-APORT devices. If your part has an APORT
 * crossbar, edit s_adc_channels[] below to use `posSel`/
 * adcPosSelAPORT* constants instead — the same "EDIT ME, board/
 * part-specific" treatment this vendor's I2C SCL/SDA pin table
 * already gets.
 *
 * ADC_IF_SINGLE/ADC_IEN_SINGLE (the single-conversion-data-valid
 * interrupt flag) follow emlib's well-established IF_<event> naming
 * convention (already relied on for this vendor's USART_IF_TXBL/
 * RXDATAV) but weren't independently re-confirmed in this fetch pass
 * — flagged the same way this vendor's other files flag their own
 * unconfirmed specifics.
 *
 * Same clock-enable bug this vendor's GPIO adapter had: ADC_Init() does
 * not enable the peripheral's own bus clock — hf_adc_init() now enables
 * the decoded instance's ADC clock unconditionally before ADC_Init(),
 * idempotent same as the GPIO fix.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_ADC

#include "em_adc.h"
#include "em_cmu.h"

#ifdef ADC0
#define HF_SILABS_ADC_0 ADC0
#else
#define HF_SILABS_ADC_0 NULL
#endif
#ifdef ADC1
#define HF_SILABS_ADC_1 ADC1
#else
#define HF_SILABS_ADC_1 NULL
#endif

static ADC_TypeDef * const s_adc_table[] = { HF_SILABS_ADC_0, HF_SILABS_ADC_1 };
#define HF_SILABS_ADC_COUNT (sizeof(s_adc_table) / sizeof(s_adc_table[0]))

/* EDIT ME if your part uses an APORT crossbar — see file header
 * comment. These are the simple, non-APORT channel constants. */
static const ADC_SingleInput_TypeDef s_adc_channels[] = {
    adcSingleInputCh0, adcSingleInputCh1, adcSingleInputCh2, adcSingleInputCh3,
    adcSingleInputCh4, adcSingleInputCh5, adcSingleInputCh6, adcSingleInputCh7,
};
#define HF_SILABS_ADC_CHANNEL_COUNT (sizeof(s_adc_channels) / sizeof(s_adc_channels[0]))

static uint16_t *s_value_ptr[HF_SILABS_ADC_COUNT];
static volatile bool s_busy[HF_SILABS_ADC_COUNT];
static hf_adc_callback_t s_callback[HF_SILABS_ADC_COUNT];

static ADC_TypeDef *hf_decode_adc(hf_adc_t instance)
{
    if (instance >= HF_SILABS_ADC_COUNT || s_adc_table[instance] == NULL) {
        return NULL;
    }
    return s_adc_table[instance];
}

bool hf_adc_init(hf_adc_t instance)
{
    ADC_TypeDef *adc = hf_decode_adc(instance);
    if (adc == NULL) {
        return false;
    }
#ifdef ADC0
    if (adc == ADC0) { CMU_ClockEnable(cmuClock_ADC0, true); }
#endif
#ifdef ADC1
    if (adc == ADC1) { CMU_ClockEnable(cmuClock_ADC1, true); }
#endif

    ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
    ADC_Init(adc, &init);
    return true;
}

static bool hf_adc_config_channel(ADC_TypeDef *adc, hf_adc_channel_t channel)
{
    if (channel >= HF_SILABS_ADC_CHANNEL_COUNT) {
        return false;
    }
    ADC_InitSingle_TypeDef singleInit = ADC_INITSINGLE_DEFAULT;
    singleInit.resolution = adcRes12Bit;
    singleInit.input = s_adc_channels[channel];
    ADC_InitSingle(adc, &singleInit);
    return true;
}

bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC_TypeDef *adc = hf_decode_adc(instance);
    if (adc == NULL || !hf_adc_config_channel(adc, channel)) {
        return false;
    }
    ADC_Start(adc, adcStartSingle);
    while (!(ADC_IntGet(adc) & ADC_IF_SINGLE)) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as this vendor's other
         * blocking-loop adapters. */
    }
    ADC_IntClear(adc, ADC_IF_SINGLE);
    *value = (uint16_t)ADC_DataSingleGet(adc);
    return true;
}

void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback)
{
    if (instance < HF_SILABS_ADC_COUNT) {
        s_callback[instance] = callback;
    }
}

bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC_TypeDef *adc = hf_decode_adc(instance);
    if (adc == NULL || s_busy[instance] || !hf_adc_config_channel(adc, channel)) {
        return false;
    }
    s_value_ptr[instance] = value;
    s_busy[instance] = true;
    ADC_IntClear(adc, ADC_IF_SINGLE);
    ADC_IntEnable(adc, ADC_IEN_SINGLE);
    ADC_Start(adc, adcStartSingle);
    return true;
}

static void hf_adc_service_irq(hf_adc_t instance, ADC_TypeDef *adc)
{
    if (ADC_IntGet(adc) & ADC_IF_SINGLE) {
        ADC_IntClear(adc, ADC_IF_SINGLE);
        ADC_IntDisable(adc, ADC_IEN_SINGLE);
        *s_value_ptr[instance] = (uint16_t)ADC_DataSingleGet(adc);
        s_busy[instance] = false;
        if (s_callback[instance] != NULL) {
            s_callback[instance](instance);
        }
    }
}

#ifdef ADC0
void ADC0_IRQHandler(void)
{
    hf_adc_service_irq(0, ADC0);
}
#endif
#ifdef ADC1
void ADC1_IRQHandler(void)
{
    hf_adc_service_irq(1, ADC1);
}
#endif

#endif /* HF_ENABLE_ADC */
#endif /* TARGET_SILABS */
