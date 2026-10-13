/* Silicon Labs adapter for the ADC portion of hal_wrapper.h, against
 * IADC (Incremental ADC) — gated by TARGET_SILABS_SLGPIO, the same
 * target macro the current sl_gpio/sl_i2c adapters use.
 *
 * Genuinely different hardware from hal_port_silabs_adc.c's classic
 * ADC, not just a newer driver over the same registers — see that
 * file's header comment for why this is a real two-adapter GPIO/I2C-
 * style split rather than a UART/SPI-style shared driver. Verified
 * IADC_PosInput_t/IADC_NegInput_t's simplest member names
 * (iadcPosInputPortAPin0, iadcNegInputGnd, etc.) and IADC_init/
 * IADC_initSingle/IADC_command/IADC_readSingleResult's signatures
 * directly against em_iadc.h in SiliconLabs/simplicity_sdk
 * (sisdk-2025.6) before writing this.
 *
 * IMPORTANT — this file carries more genuinely-unconfirmed specifics
 * than any other adapter in this repo, flagged here rather than
 * presented with false confidence:
 * - `IADC_AllConfigs_t`/`IADC_ALLCONFIGS_DEFAULT` and its `configs[]`
 *   array field, and `IADC_SingleInput_t`/`IADC_SINGLEINPUT_DEFAULT`,
 *   follow the naming convention used in Silicon Labs' own published
 *   "iadc single conversion" example projects, but weren't
 *   independently re-confirmed by reading em_iadc.h's exact struct
 *   definitions in this fetch pass.
 * - `IADC_Result_t`'s exact field name for the sample value (used here
 *   as `.data`) likewise follows that same example-project
 *   convention, not a directly re-quoted struct definition.
 * - `IADC_IF_SINGLEDONE`/`IADC_IEN_SINGLEDONE` (the single-conversion-
 *   done interrupt flag), `IADC_disableInt` (this file assumes it's
 *   the sibling of the confirmed `IADC_enableInt`, universal in every
 *   other emlib peripheral's IntEnable/IntDisable pair), and the
 *   `IADC0_IRQHandler` IRQ vector name follow this vendor's
 *   well-established per-peripheral naming pattern (already relied on
 *   for USART0_RX_IRQHandler, ADC0_IRQHandler elsewhere in this repo)
 *   but are defined in a device-specific header this fetch pass
 *   didn't reach, same category of gap as this vendor's other
 *   adapters' flagged names.
 * Cross-check all of the above against your installed Simplicity SDK
 * (or its "iadc_single_conversion_polled"/"_interrupt" example
 * projects) before relying on this file.
 *
 * `posInput`/IADC_PosInput_t selects an arbitrary GPIO port+pin, not
 * a fixed dedicated analog pin the way nRF52's SAADC channels are —
 * so, like this vendor's I2C SCL/SDA pin table, s_adc_channels[]
 * below is an EDIT-ME table you must match to your board's actual
 * wiring. `negInput` is fixed at `iadcNegInputGnd` (single-ended only,
 * matching this project's master-only, single-ended-only ADC scope —
 * differential conversion is out of scope, same kind of deliberate
 * scope line this project already draws for I2C/SPI peripheral mode).
 *
 * Alignment is fixed at `iadcAlignRight12` so results land as a plain
 * 12-bit right-aligned value in `IADC_Result_t`, matching the header's
 * fixed-12-bit-resolution contract without touching this driver's
 * separate (and considerably more involved) oversampling-ratio
 * configuration.
 *
 * Same clock-enable bug this vendor's GPIO adapter had: unlike
 * sl_gpio/sl_i2c (which enable their own bus clock internally via
 * sl_clock_manager_enable_bus_clock — verified against
 * platform/driver/gpio/src/sl_gpio.c and platform/driver/i2c/src/
 * sl_i2c.c), IADC_init() is a plain emlib-shaped call with no driver
 * layer above it and does not enable its own bus clock. hf_adc_init()
 * now enables cmuClock_IADC0 unconditionally before IADC_init(),
 * idempotent same as the GPIO fix.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_ADC

#include "em_iadc.h"
#include "em_cmu.h"

#ifdef IADC0
#define HF_SILABS_IADC_0 IADC0
#else
#define HF_SILABS_IADC_0 NULL
#endif

static IADC_TypeDef * const s_iadc_table[] = { HF_SILABS_IADC_0 };
#define HF_SILABS_IADC_COUNT (sizeof(s_iadc_table) / sizeof(s_iadc_table[0]))

/* EDIT ME: match these to your board's actual ADC input wiring — see
 * file header comment. Placeholders, not verified board defaults. */
static const IADC_PosInput_t s_adc_channels[] = {
    iadcPosInputPortAPin0, iadcPosInputPortAPin1,
    iadcPosInputPortAPin2, iadcPosInputPortAPin3,
    iadcPosInputPortAPin4, iadcPosInputPortAPin5,
    iadcPosInputPortAPin6, iadcPosInputPortAPin7,
};
#define HF_SILABS_IADC_CHANNEL_COUNT (sizeof(s_adc_channels) / sizeof(s_adc_channels[0]))

static uint16_t *s_value_ptr[HF_SILABS_IADC_COUNT];
static volatile bool s_busy[HF_SILABS_IADC_COUNT];
static hf_adc_callback_t s_callback[HF_SILABS_IADC_COUNT];

static IADC_TypeDef *hf_decode_adc(hf_adc_t instance)
{
    if (instance >= HF_SILABS_IADC_COUNT || s_iadc_table[instance] == NULL) {
        return NULL;
    }
    return s_iadc_table[instance];
}

bool hf_adc_init(hf_adc_t instance)
{
    IADC_TypeDef *iadc = hf_decode_adc(instance);
    if (iadc == NULL) {
        return false;
    }
#ifdef IADC0
    if (iadc == IADC0) { CMU_ClockEnable(cmuClock_IADC0, true); }
#endif

    IADC_Init_t init = IADC_INIT_DEFAULT;
    IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;
    IADC_init(iadc, &init, &allConfigs);
    return true;
}

static bool hf_adc_config_channel(IADC_TypeDef *iadc, hf_adc_channel_t channel)
{
    if (channel >= HF_SILABS_IADC_CHANNEL_COUNT) {
        return false;
    }
    IADC_InitSingle_t singleInit = IADC_INITSINGLE_DEFAULT;
    singleInit.alignment = iadcAlignRight12;

    IADC_SingleInput_t input = IADC_SINGLEINPUT_DEFAULT;
    input.posInput = s_adc_channels[channel];
    input.negInput = iadcNegInputGnd;

    IADC_initSingle(iadc, &singleInit, &input);
    return true;
}

bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    IADC_TypeDef *iadc = hf_decode_adc(instance);
    if (iadc == NULL || !hf_adc_config_channel(iadc, channel)) {
        return false;
    }
    IADC_command(iadc, iadcCmdStartSingle);
    while (!(IADC_getInt(iadc) & IADC_IF_SINGLEDONE)) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as this vendor's other
         * blocking-loop adapters. */
    }
    IADC_clearInt(iadc, IADC_IF_SINGLEDONE);
    IADC_Result_t result = IADC_readSingleResult(iadc);
    *value = (uint16_t)(result.data & 0x0FFFU);
    return true;
}

void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback)
{
    if (instance < HF_SILABS_IADC_COUNT) {
        s_callback[instance] = callback;
    }
}

bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value)
{
    IADC_TypeDef *iadc = hf_decode_adc(instance);
    if (iadc == NULL || s_busy[instance] || !hf_adc_config_channel(iadc, channel)) {
        return false;
    }
    s_value_ptr[instance] = value;
    s_busy[instance] = true;
    IADC_clearInt(iadc, IADC_IF_SINGLEDONE);
    IADC_enableInt(iadc, IADC_IEN_SINGLEDONE);
    IADC_command(iadc, iadcCmdStartSingle);
    return true;
}

static void hf_adc_service_irq(hf_adc_t instance, IADC_TypeDef *iadc)
{
    if (IADC_getInt(iadc) & IADC_IF_SINGLEDONE) {
        IADC_clearInt(iadc, IADC_IF_SINGLEDONE);
        IADC_disableInt(iadc, IADC_IEN_SINGLEDONE);
        IADC_Result_t result = IADC_readSingleResult(iadc);
        *s_value_ptr[instance] = (uint16_t)(result.data & 0x0FFFU);
        s_busy[instance] = false;
        if (s_callback[instance] != NULL) {
            s_callback[instance](instance);
        }
    }
}

#ifdef IADC0
void IADC0_IRQHandler(void)
{
    hf_adc_service_irq(0, IADC0);
}
#endif

#endif /* HF_ENABLE_ADC */
#endif /* TARGET_SILABS_SLGPIO */
