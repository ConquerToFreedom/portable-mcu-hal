/* Fake stand-in for Silicon Labs emlib's em_adc.h, used ONLY to
 * host-compile and unit-test this vendor's classic ADC adapter under
 * silicon_labs/adc/hal_port_silabs_adc.c (TARGET_SILABS). Not a copy
 * of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_EM_ADC_H
#define HAL_WRAPPER_FAKE_EM_ADC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int _opaque; } ADC_TypeDef;

typedef struct { int _unused; } ADC_Init_TypeDef;
#define ADC_INIT_DEFAULT { 0 }

typedef enum { adcRes12Bit } ADC_Res_TypeDef;

typedef enum {
    adcSingleInputCh0, adcSingleInputCh1, adcSingleInputCh2, adcSingleInputCh3,
    adcSingleInputCh4, adcSingleInputCh5, adcSingleInputCh6, adcSingleInputCh7
} ADC_SingleInput_TypeDef;

typedef struct {
    ADC_Res_TypeDef resolution;
    ADC_SingleInput_TypeDef input;
} ADC_InitSingle_TypeDef;

#define ADC_INITSINGLE_DEFAULT { .resolution = adcRes12Bit, .input = adcSingleInputCh0 }

typedef enum { adcStartSingle } ADC_Start_TypeDef;

#define ADC_IF_SINGLE  (1U << 0)
#define ADC_IEN_SINGLE ADC_IF_SINGLE

extern ADC_TypeDef fake_silabs_adc0;
extern ADC_TypeDef fake_silabs_adc1;
#define ADC0 (&fake_silabs_adc0)
#define ADC1 (&fake_silabs_adc1)

void ADC_Init(ADC_TypeDef *adc, ADC_Init_TypeDef const *init);
void ADC_InitSingle(ADC_TypeDef *adc, ADC_InitSingle_TypeDef const *init);
void ADC_Start(ADC_TypeDef *adc, ADC_Start_TypeDef cmd);
uint32_t ADC_IntGet(ADC_TypeDef *adc);
void ADC_IntClear(ADC_TypeDef *adc, uint32_t flags);
void ADC_IntEnable(ADC_TypeDef *adc, uint32_t flags);
void ADC_IntDisable(ADC_TypeDef *adc, uint32_t flags);
uint32_t ADC_DataSingleGet(ADC_TypeDef *adc);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_ADC_H */
