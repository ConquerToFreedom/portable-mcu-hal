/* Fake stand-in for Silicon Labs emlib's em_iadc.h, used ONLY to
 * host-compile and unit-test this vendor's IADC adapter under
 * silicon_labs/adc/hal_port_silabs_slgpio_adc.c (TARGET_SILABS_SLGPIO).
 * Not a copy of Silicon Labs' header - and this adapter's own file
 * header already flags more unconfirmed specifics than any other
 * adapter in this project, so this fake is correspondingly the most
 * speculative of this repo's fakes too. Cross-check both against a
 * real Simplicity SDK before trusting either.
 */
#ifndef HAL_WRAPPER_FAKE_EM_IADC_H
#define HAL_WRAPPER_FAKE_EM_IADC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int _opaque; } IADC_TypeDef;

extern IADC_TypeDef fake_silabs_iadc0;
#define IADC0 (&fake_silabs_iadc0)

typedef struct { int _unused; } IADC_Init_t;
#define IADC_INIT_DEFAULT { 0 }

typedef struct { int configs[1]; } IADC_AllConfigs_t;
#define IADC_ALLCONFIGS_DEFAULT { { 0 } }

typedef enum { iadcAlignRight12 } IADC_Alignment_t;

typedef struct {
    IADC_Alignment_t alignment;
} IADC_InitSingle_t;
#define IADC_INITSINGLE_DEFAULT { .alignment = iadcAlignRight12 }

typedef enum {
    iadcPosInputPortAPin0, iadcPosInputPortAPin1, iadcPosInputPortAPin2, iadcPosInputPortAPin3,
    iadcPosInputPortAPin4, iadcPosInputPortAPin5, iadcPosInputPortAPin6, iadcPosInputPortAPin7
} IADC_PosInput_t;

typedef enum { iadcNegInputGnd } IADC_NegInput_t;

typedef struct {
    IADC_PosInput_t posInput;
    IADC_NegInput_t negInput;
} IADC_SingleInput_t;
#define IADC_SINGLEINPUT_DEFAULT { .posInput = iadcPosInputPortAPin0, .negInput = iadcNegInputGnd }

typedef enum { iadcCmdStartSingle } IADC_Command_t;

#define IADC_IF_SINGLEDONE  (1U << 0)
#define IADC_IEN_SINGLEDONE IADC_IF_SINGLEDONE

typedef struct {
    uint32_t data;
} IADC_Result_t;

void IADC_init(IADC_TypeDef *iadc, IADC_Init_t const *init, IADC_AllConfigs_t const *all_configs);
void IADC_initSingle(IADC_TypeDef *iadc, IADC_InitSingle_t const *init, IADC_SingleInput_t const *input);
void IADC_command(IADC_TypeDef *iadc, IADC_Command_t cmd);
uint32_t IADC_getInt(IADC_TypeDef *iadc);
void IADC_clearInt(IADC_TypeDef *iadc, uint32_t flags);
void IADC_enableInt(IADC_TypeDef *iadc, uint32_t flags);
void IADC_disableInt(IADC_TypeDef *iadc, uint32_t flags);
IADC_Result_t IADC_readSingleResult(IADC_TypeDef *iadc);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_IADC_H */
