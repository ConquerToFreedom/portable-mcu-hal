/* Silicon Labs (emlib) vendor adapter for the UART portion of
 * hal_wrapper.h — the classic/legacy API, gated by TARGET_SILABS, same
 * split as the GPIO/I2C adapters (see hal_port_silabs_slgpio_uart.c
 * for the current sl_iostream_usart driver instead).
 *
 * `USART_Tx`/`USART_Rx` are already blocking single-byte primitives
 * (spin internally until the buffer/data register is ready), so
 * unlike STM32/nRF52 this adapter's blocking hf_uart_write() needs no
 * interrupt involvement at all — just a loop. The async tier and the
 * background RX path are the only places interrupts get used here.
 *
 * TX completion needs two different interrupt flags in sequence:
 * USART_IEN_TXBL (transmit buffer level — fires when the buffer has
 * room for the next byte) feeds bytes one at a time, then once the
 * last byte has been handed to the shift register this switches to
 * USART_IEN_TXC (transmit complete — fires once the byte, including
 * its stop bit, has actually left the wire) for the real completion
 * signal. Feeding bytes off of TXBL alone and calling that "done"
 * would report completion one byte-time too early.
 *
 * Hardware-sharing note: this same USART peripheral is also what
 * ../spi/hal_port_silabs_spi.c uses (classic EFM32/EFR32 SPI runs on
 * the same USART hardware, via SPIDRV, as UART) — enabling both files
 * for the SAME USART instance produces a duplicate interrupt-vector
 * link error. Trim each file's instance table to disjoint instances
 * if you need both peripherals; see the SPI file's own header comment
 * for the full explanation. (SPIDRV enables its own USART bus clock
 * internally — verified against platform/emdrv/spidrv/src/spidrv.c in
 * SiliconLabs/simplicity_sdk — so that file needs no clock-enable fix
 * of its own; this one, calling USART_InitAsync() directly with no
 * driver layer above it, does.)
 *
 * Like this vendor's GPIO adapter, USART_InitAsync() does not enable
 * its own peripheral clock — confirmed absent from
 * SiliconLabs/gecko_sdk's platform/emlib/inc/em_cmu.h and by the same
 * "every classic emlib peripheral needs its own CMU_ClockEnable()"
 * pattern found there. hf_uart_init() now enables the decoded
 * instance's USART clock unconditionally before USART_InitAsync() —
 * idempotent, same reasoning as the GPIO fix.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_UART

#include "em_usart.h"
#include "em_cmu.h"

#ifdef USART0
#define HF_SILABS_USART_0 USART0
#else
#define HF_SILABS_USART_0 NULL
#endif
#ifdef USART1
#define HF_SILABS_USART_1 USART1
#else
#define HF_SILABS_USART_1 NULL
#endif

static USART_TypeDef * const s_usart_table[] = { HF_SILABS_USART_0, HF_SILABS_USART_1 };
#define HF_SILABS_UART_COUNT (sizeof(s_usart_table) / sizeof(s_usart_table[0]))

#define HF_UART_RX_BUFFER_SIZE 128

typedef struct {
    uint8_t buffer[HF_UART_RX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} hf_uart_rx_ring_t;

static hf_uart_rx_ring_t s_rx_ring[HF_SILABS_UART_COUNT];
static const uint8_t *s_tx_data[HF_SILABS_UART_COUNT];
static uint32_t s_tx_len[HF_SILABS_UART_COUNT];
static uint32_t s_tx_pos[HF_SILABS_UART_COUNT];
static volatile bool s_tx_busy[HF_SILABS_UART_COUNT];
static hf_uart_tx_callback_t s_tx_callback[HF_SILABS_UART_COUNT];
static hf_uart_rx_callback_t s_rx_callback[HF_SILABS_UART_COUNT];

static void hf_uart_rx_ring_push(hf_uart_rx_ring_t *ring, uint8_t byte)
{
    uint32_t next = (ring->head + 1U) % HF_UART_RX_BUFFER_SIZE;
    if (next != ring->tail) {
        ring->buffer[ring->head] = byte;
        ring->head = next;
    }
}

static uint32_t hf_uart_rx_ring_pop(hf_uart_rx_ring_t *ring, uint8_t *data, uint32_t len)
{
    uint32_t count = 0;
    while (count < len && ring->tail != ring->head) {
        data[count++] = ring->buffer[ring->tail];
        ring->tail = (ring->tail + 1U) % HF_UART_RX_BUFFER_SIZE;
    }
    return count;
}

static USART_TypeDef *hf_decode_uart(hf_uart_t instance)
{
    if (instance >= HF_SILABS_UART_COUNT) {
        return NULL;
    }
    return s_usart_table[instance];
}

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate)
{
    USART_TypeDef *usart = hf_decode_uart(instance);
    if (usart == NULL) {
        return false;
    }

#ifdef USART0
    if (usart == USART0) { CMU_ClockEnable(cmuClock_USART0, true); }
#endif
#ifdef USART1
    if (usart == USART1) { CMU_ClockEnable(cmuClock_USART1, true); }
#endif

    USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;
    init.baudrate = baud_rate;
    USART_InitAsync(usart, &init);

    USART_IntClear(usart, USART_IF_RXDATAV);
    USART_IntEnable(usart, USART_IEN_RXDATAV);

#ifdef USART0
    if (usart == USART0) {
        NVIC_EnableIRQ(USART0_RX_IRQn);
        NVIC_EnableIRQ(USART0_TX_IRQn);
    }
#endif
#ifdef USART1
    if (usart == USART1) {
        NVIC_EnableIRQ(USART1_RX_IRQn);
        NVIC_EnableIRQ(USART1_TX_IRQn);
    }
#endif
    return true;
}

void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    USART_TypeDef *usart = hf_decode_uart(instance);
    if (usart == NULL) {
        return;
    }
    uint32_t i;
    for (i = 0; i < len; i++) {
        USART_Tx(usart, data[i]);
    }
}

uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len)
{
    if (instance >= HF_SILABS_UART_COUNT) {
        return 0;
    }
    return hf_uart_rx_ring_pop(&s_rx_ring[instance], data, len);
}

void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback)
{
    if (instance < HF_SILABS_UART_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback)
{
    if (instance < HF_SILABS_UART_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    USART_TypeDef *usart = hf_decode_uart(instance);
    if (usart == NULL || s_tx_busy[instance] || len == 0) {
        return false;
    }
    s_tx_data[instance] = data;
    s_tx_len[instance] = len;
    s_tx_pos[instance] = 0;
    s_tx_busy[instance] = true;

    USART_IntClear(usart, USART_IF_TXBL | USART_IF_TXC);
    USART_IntEnable(usart, USART_IEN_TXBL);
    return true;
}

static void hf_uart_service_rx_irq(hf_uart_t instance, USART_TypeDef *usart)
{
    if (USART_IntGet(usart) & USART_IF_RXDATAV) {
        uint8_t byte = (uint8_t)usart->RXDATA;
        USART_IntClear(usart, USART_IF_RXDATAV);
        hf_uart_rx_ring_push(&s_rx_ring[instance], byte);
        if (s_rx_callback[instance] != NULL) {
            s_rx_callback[instance](instance);
        }
    }
}

static void hf_uart_service_tx_irq(hf_uart_t instance, USART_TypeDef *usart)
{
    uint32_t flags = USART_IntGet(usart);

    if (flags & USART_IF_TXBL) {
        USART_IntClear(usart, USART_IF_TXBL);
        if (s_tx_pos[instance] < s_tx_len[instance]) {
            usart->TXDATA = s_tx_data[instance][s_tx_pos[instance]];
            s_tx_pos[instance]++;
        }
        if (s_tx_pos[instance] >= s_tx_len[instance]) {
            USART_IntDisable(usart, USART_IEN_TXBL);
            USART_IntEnable(usart, USART_IEN_TXC);
        }
    }

    if (flags & USART_IF_TXC) {
        USART_IntClear(usart, USART_IF_TXC);
        USART_IntDisable(usart, USART_IEN_TXC);
        s_tx_busy[instance] = false;
        if (s_tx_callback[instance] != NULL) {
            s_tx_callback[instance](instance);
        }
    }
}

#ifdef USART0
void USART0_RX_IRQHandler(void)
{
    hf_uart_service_rx_irq(0, USART0);
}

void USART0_TX_IRQHandler(void)
{
    hf_uart_service_tx_irq(0, USART0);
}
#endif
#ifdef USART1
void USART1_RX_IRQHandler(void)
{
    hf_uart_service_rx_irq(1, USART1);
}

void USART1_TX_IRQHandler(void)
{
    hf_uart_service_tx_irq(1, USART1);
}
#endif

#endif /* HF_ENABLE_UART */
#endif /* TARGET_SILABS */
