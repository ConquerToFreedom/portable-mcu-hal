/* Fake stand-in for Silicon Labs' SPIDRV (platform/emdrv/spidrv), used
 * ONLY to host-compile and unit-test this vendor's SPI adapters (both
 * silicon_labs/spi/hal_port_silabs_spi.c under TARGET_SILABS and
 * silicon_labs/spi/hal_port_silabs_slgpio_spi.c under
 * TARGET_SILABS_SLGPIO - SPIDRV is used unchanged by both, see those
 * files' own header comments). Not a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_SPIDRV_H
#define HAL_WRAPPER_FAKE_SPIDRV_H

#include <stdint.h>
#include <stddef.h>
/* Real SPIDRV resolves its `port` field against the same USART/EUSART
 * peripheral blocks em_usart.h declares (that's the whole point of the
 * shared-hardware finding in the SPI adapters' own header comments) -
 * this fake needs USART0/USART1 defined for the same reason. */
#include "em_usart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t Ecode_t;
#define ECODE_OK 0

typedef enum { spidrvClockMode0, spidrvClockMode1, spidrvClockMode2, spidrvClockMode3 } SPIDRV_ClockMode_t;
typedef enum { spidrvMaster } SPIDRV_Type_t;
typedef enum { spidrvBitOrderMsbFirst, spidrvBitOrderLsbFirst } SPIDRV_BitOrder_t;
typedef enum { spidrvCsControlApplication, spidrvCsControlAuto } SPIDRV_CsControl_t;

typedef struct {
    void *port;
    uint32_t bitRate;
    uint32_t frameLength;
    uint32_t dummyTxValue;
    SPIDRV_Type_t type;
    SPIDRV_BitOrder_t bitOrder;
    SPIDRV_ClockMode_t clockMode;
    SPIDRV_CsControl_t csControl;
} SPIDRV_Init_t;

typedef struct { int _opaque; } SPIDRV_HandleData_t;
typedef SPIDRV_HandleData_t *SPIDRV_Handle_t;

typedef void (*SPIDRV_Callback_t)(SPIDRV_Handle_t handle, Ecode_t transferStatus, int itemsTransferred);

Ecode_t SPIDRV_Init(SPIDRV_Handle_t handle, SPIDRV_Init_t *initData);
Ecode_t SPIDRV_MTransmitB(SPIDRV_Handle_t handle, void const *buffer, int count);
Ecode_t SPIDRV_MReceiveB(SPIDRV_Handle_t handle, void *buffer, int count);
Ecode_t SPIDRV_MTransferB(SPIDRV_Handle_t handle, void const *txBuffer, void *rxBuffer, int count);
Ecode_t SPIDRV_MTransmit(SPIDRV_Handle_t handle, void const *buffer, int count, SPIDRV_Callback_t callback);
Ecode_t SPIDRV_MReceive(SPIDRV_Handle_t handle, void *buffer, int count, SPIDRV_Callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SPIDRV_H */
