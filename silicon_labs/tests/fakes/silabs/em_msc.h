/* Fake stand-in for Silicon Labs emlib's em_msc.h, used ONLY to
 * host-compile and unit-test this vendor's Flash adapters under
 * silicon_labs/flash/ — BOTH hal_port_silabs_flash.c (TARGET_SILABS)
 * and hal_port_silabs_slgpio_flash.c (TARGET_SILABS_SLGPIO), which
 * use em_msc.h directly (see that second file's own header comment
 * for why there's no distinct current-SDK driver at this level). Not
 * a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_EM_MSC_H
#define HAL_WRAPPER_FAKE_EM_MSC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    mscReturnOk        = 0,
    mscReturnInvalidAddr = 1,
    mscReturnLocked     = 2,
    mscReturnTimeOut    = 3,
    mscReturnUnaligned  = 4
} msc_Return_TypeDef;

void MSC_Init(void);
void MSC_Deinit(void);
msc_Return_TypeDef MSC_ErasePage(uint32_t *startAddress);
msc_Return_TypeDef MSC_WriteWord(uint32_t *address, const void *data, uint32_t numBytes);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_MSC_H */
