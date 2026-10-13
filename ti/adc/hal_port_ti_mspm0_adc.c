/* TI MSPM0 (driverlib) vendor adapter for the ADC portion of
 * hal_wrapper.h, against DL_ADC12. Assumes the customer's
 * SysConfig-generated "ti_msp_dl_config.h" is on the include path,
 * same assumption as this vendor's other adapters.
 *
 * Verified this file's core API directly against
 * TexasInstruments/mspm0-sdk's own source/ti/driverlib/dl_adc12.h on
 * GitHub, same higher-confidence source this vendor's SPI adapter
 * used (TI's docs domain and forum are both blocked in this
 * environment) — confirmed: DL_ADC12_initSingleSample's full
 * parameter list, DL_ADC12_enablePower/reset/enableConversions,
 * DL_ADC12_setSampleTime0, DL_ADC12_startConversion,
 * DL_ADC12_getMemResult, DL_ADC12_configConversionMem's full
 * parameter list, DL_ADC12_getPendingInterrupt and the
 * DL_ADC12_IIDX_MEMn_RESULT_LOADED constants, and the
 * DL_ADC12_SAMP_CONV_RES_12/10/8_BIT resolution constants.
 *
 * Real finding, different in shape from this vendor's other three
 * peripherals: ADC12 results are indexed by a conversion-memory slot
 * (DL_ADC12_MEM_IDX, 0-11), not read back by raw channel number —
 * each MEM_IDX is configured, via DL_ADC12_configConversionMem(), to
 * sample one specific channel. SysConfig's usual pattern is to
 * pre-configure a fixed MEM_IDX-to-channel mapping once in
 * ti_msp_dl_config.c and leave it alone. This adapter instead
 * reconfigures MEM_IDX 0 to the requested channel on every
 * hf_adc_read()/read_async() call — a deliberate choice, not a silent
 * workaround, needed because hf_adc_channel_t is meant to select an
 * arbitrary channel per call, the same "one call, one discrete
 * operation" shape every other peripheral in this header already
 * uses. `channel` maps directly onto DL_ADC12_INPUT_CHAN_0..15
 * (confirmed these constants exist up to at least _CHAN_15, with
 * _CHAN_31 on parts with more channels).
 *
 * DL_ADC12_configConversionMem()'s six parameters beyond
 * adc12/idx/chansel — vref, stime, avgen, bcsen, trig, wincomp —
 * were confirmed to exist in that exact order and count, but their
 * individual enum-constant values were NOT independently confirmed in
 * this fetch pass (the header's full enum listings for each weren't
 * reached). This file passes conservative, disabled/software-default-
 * style values for all six; cross-check each against your installed
 * SDK's dl_adc12.h before relying on this file. Two similarly
 * unconfirmed choices in the init sequence: whether
 * DL_ADC12_SAMPLING_SOURCE_AUTO or `_MANUAL` is the right pick for a
 * single software-triggered conversion (this file uses `_AUTO`,
 * matching most published single-conversion examples, but the
 * distinction itself wasn't independently confirmed), and the sample
 * time passed to DL_ADC12_setSampleTime0() (an arbitrary
 * conservative constant, not tuned to any specific part's ADCCLK).
 * DL_ADC12_disableInterrupt is assumed to be the sibling of the
 * confirmed DL_ADC12_enableInterrupt, following the same
 * enable/disable pairing convention already flagged the same way in
 * this vendor's SPI adapter (DL_SPI_disableInterrupt).
 *
 * hf_adc_write()/read() drain a single MEM_IDX one shot at a time (no
 * whole-sequence multi-channel primitive is used here), matching this
 * header's fixed "one channel, one call" scope.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_ADC

#include "ti_msp_dl_config.h"

#ifdef ADC0
#define HF_MSPM0_ADC_0 ADC0
#else
#define HF_MSPM0_ADC_0 NULL
#endif
#ifdef ADC1
#define HF_MSPM0_ADC_1 ADC1
#else
#define HF_MSPM0_ADC_1 NULL
#endif

static ADC12_Regs * const s_adc_table[] = { HF_MSPM0_ADC_0, HF_MSPM0_ADC_1 };
#define HF_MSPM0_ADC_COUNT (sizeof(s_adc_table) / sizeof(s_adc_table[0]))

/* channel maps directly onto DL_ADC12_INPUT_CHAN_0..15 — see file
 * header comment. */
static const uint32_t s_adc_channels[] = {
    DL_ADC12_INPUT_CHAN_0,  DL_ADC12_INPUT_CHAN_1,  DL_ADC12_INPUT_CHAN_2,  DL_ADC12_INPUT_CHAN_3,
    DL_ADC12_INPUT_CHAN_4,  DL_ADC12_INPUT_CHAN_5,  DL_ADC12_INPUT_CHAN_6,  DL_ADC12_INPUT_CHAN_7,
    DL_ADC12_INPUT_CHAN_8,  DL_ADC12_INPUT_CHAN_9,  DL_ADC12_INPUT_CHAN_10, DL_ADC12_INPUT_CHAN_11,
    DL_ADC12_INPUT_CHAN_12, DL_ADC12_INPUT_CHAN_13, DL_ADC12_INPUT_CHAN_14, DL_ADC12_INPUT_CHAN_15,
};
#define HF_MSPM0_ADC_CHANNEL_COUNT (sizeof(s_adc_channels) / sizeof(s_adc_channels[0]))

#define HF_MSPM0_ADC_MEM_IDX DL_ADC12_MEM_IDX_0

static uint16_t *s_value_ptr[HF_MSPM0_ADC_COUNT];
static volatile bool s_busy[HF_MSPM0_ADC_COUNT];
static hf_adc_callback_t s_callback[HF_MSPM0_ADC_COUNT];

static ADC12_Regs *hf_decode_adc(hf_adc_t instance)
{
    if (instance >= HF_MSPM0_ADC_COUNT) {
        return NULL;
    }
    return s_adc_table[instance];
}

bool hf_adc_init(hf_adc_t instance)
{
    ADC12_Regs *adc = hf_decode_adc(instance);
    if (adc == NULL) {
        return false;
    }
    DL_ADC12_reset(adc);
    DL_ADC12_enablePower(adc);
    DL_ADC12_setSampleTime0(adc, 20U); /* conservative default — see file header comment */
    DL_ADC12_initSingleSample(adc, DL_ADC12_REPEAT_MODE_DISABLED,
                               DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
                               DL_ADC12_SAMP_CONV_RES_12_BIT,
                               DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_enableConversions(adc);
    return true;
}

static bool hf_adc_config_channel(ADC12_Regs *adc, hf_adc_channel_t channel)
{
    if (channel >= HF_MSPM0_ADC_CHANNEL_COUNT) {
        return false;
    }
    /* vref/stime/avgen/bcsen/trig/wincomp — conservative disabled-
     * style defaults, not independently confirmed. See file header
     * comment. */
    DL_ADC12_configConversionMem(adc, HF_MSPM0_ADC_MEM_IDX, s_adc_channels[channel],
                                  DL_ADC12_REFERENCE_VOLTAGE_VDDA,
                                  DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
                                  DL_ADC12_AVERAGING_MODE_DISABLED,
                                  DL_ADC12_BURN_OUT_SOURCE_DISABLED,
                                  DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
                                  DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    return true;
}

bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC12_Regs *adc = hf_decode_adc(instance);
    if (adc == NULL || !hf_adc_config_channel(adc, channel)) {
        return false;
    }
    DL_ADC12_startConversion(adc);
    while (!(DL_ADC12_getPendingInterrupt(adc) & DL_ADC12_IIDX_MEM0_RESULT_LOADED)) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as this vendor's other
         * blocking-loop adapters. */
    }
    *value = DL_ADC12_getMemResult(adc, HF_MSPM0_ADC_MEM_IDX);
    return true;
}

void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback)
{
    if (instance < HF_MSPM0_ADC_COUNT) {
        s_callback[instance] = callback;
    }
}

bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    ADC12_Regs *adc = hf_decode_adc(instance);
    if (adc == NULL || s_busy[instance] || !hf_adc_config_channel(adc, channel)) {
        return false;
    }
    s_value_ptr[instance] = value;
    s_busy[instance] = true;
    DL_ADC12_enableInterrupt(adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_startConversion(adc);
    return true;
}

static void hf_adc_service_irq(hf_adc_t instance, ADC12_Regs *adc)
{
    if (DL_ADC12_getPendingInterrupt(adc) == DL_ADC12_IIDX_MEM0_RESULT_LOADED) {
        DL_ADC12_disableInterrupt(adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        *s_value_ptr[instance] = DL_ADC12_getMemResult(adc, HF_MSPM0_ADC_MEM_IDX);
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
#endif /* TARGET_TI_MSPM0 */
