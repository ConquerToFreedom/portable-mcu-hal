/* Fake stand-in for Silicon Labs emlib's em_usart.h, used ONLY to
 * host-compile and unit-test this vendor's UART adapters (both the
 * classic silicon_labs/uart/hal_port_silabs_uart.c under TARGET_SILABS
 * and the current silicon_labs/uart/hal_port_silabs_slgpio_uart.c
 * under TARGET_SILABS_SLGPIO - they share this exact same real emlib
 * USART peripheral, see that file's own header comment). Not a copy of
 * Silicon Labs' header. Also backs SPI's spidrv.h fake, since SPIDRV
 * manages the same USART hardware internally on this vendor.
 */
#ifndef HAL_WRAPPER_FAKE_EM_USART_H
#define HAL_WRAPPER_FAKE_EM_USART_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t RXDATA;
    volatile uint32_t TXDATA;
} USART_TypeDef;

typedef struct {
    uint32_t baudrate;
} USART_InitAsync_TypeDef;

#define USART_INITASYNC_DEFAULT { .baudrate = 115200U }

#define USART_IF_RXDATAV (1U << 0)
#define USART_IF_TXBL    (1U << 1)
#define USART_IF_TXC     (1U << 2)
#define USART_IEN_RXDATAV USART_IF_RXDATAV
#define USART_IEN_TXBL    USART_IF_TXBL
#define USART_IEN_TXC     USART_IF_TXC

extern USART_TypeDef fake_silabs_usart0;
extern USART_TypeDef fake_silabs_usart1;
#define USART0 (&fake_silabs_usart0)
#define USART1 (&fake_silabs_usart1)

#define USART0_RX_IRQn 10
#define USART0_TX_IRQn 11
#define USART1_RX_IRQn 12
#define USART1_TX_IRQn 13

void NVIC_EnableIRQ(int irqn);

void USART_InitAsync(USART_TypeDef *usart, USART_InitAsync_TypeDef const *init);
void USART_IntClear(USART_TypeDef *usart, uint32_t flags);
void USART_IntEnable(USART_TypeDef *usart, uint32_t flags);
void USART_IntDisable(USART_TypeDef *usart, uint32_t flags);
uint32_t USART_IntGet(USART_TypeDef *usart);
void USART_Tx(USART_TypeDef *usart, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_USART_H */
