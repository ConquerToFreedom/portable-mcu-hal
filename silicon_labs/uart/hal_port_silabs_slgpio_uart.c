/* Silicon Labs adapter for the UART portion of hal_wrapper.h, gated by
 * TARGET_SILABS_SLGPIO — the same target macro the current sl_gpio/
 * sl_i2c adapters use.
 *
 * Genuine finding, not a shortcut: unlike GPIO (sl_gpio) and I2C
 * (sl_i2c), Silicon Labs does not have an independent "current" UART
 * driver with its own low-level register API. The nearest equivalent,
 * platform/service/iostream's sl_iostream_usart, is a thin wrapper
 * around this exact same classic emlib USART peripheral — verified
 * directly against its source in SiliconLabs/simplicity_sdk
 * (sisdk-2025.6): sl_iostream_usart_init() takes a raw
 * USART_InitAsync_TypeDef* and a raw USART_TypeDef* pointer, the same
 * types the classic adapter (hal_port_silabs_uart.c) uses directly.
 * Worse, its generic sl_iostream_t vtable exposes no confirmed-stable
 * public primitive for a non-blocking read or a TX-completion
 * callback — sl_iostream_read()/write() are blocking, and the async
 * write path goes through an "sli_"-prefixed (Silicon Labs' own
 * internal-API convention) type not documented as stable for
 * application use.
 *
 * Using sl_iostream_usart for the blocking tier while *also* hooking
 * this file's own raw USART0_RX_IRQHandler for the background receive
 * path the async tier needs would mean two independent pieces of code
 * fighting over the same interrupt vector and RXDATA register —
 * sl_iostream_usart's own "receive during sleep" feature implies it
 * already installs interrupt handling internally. That's a real
 * correctness risk (lost bytes, or a straight duplicate-symbol link
 * conflict), not a style preference, so this file bypasses
 * sl_iostream_usart entirely and talks to the same classic emlib
 * peripheral directly — functionally identical to
 * hal_port_silabs_uart.c. See docs/architecture.md's UART section for
 * this reasoning recorded alongside the other vendor asymmetries.
 *
 * Hardware-sharing note: this same USART peripheral is also what
 * ../spi/hal_port_silabs_slgpio_spi.c uses (classic EFM32/EFR32 SPI
 * runs on the same USART hardware, via SPIDRV, as UART) — enabling
 * both files for the SAME USART instance produces a duplicate
 * interrupt-vector link error. Trim each file's instance table to
 * disjoint instances if you need both peripherals; see the SPI file's
 * own header comment for the full explanation. (SPIDRV enables its own
 * USART bus clock internally — verified against
 * platform/emdrv/spidrv/src/spidrv.c in SiliconLabs/simplicity_sdk —
 * so that file needs no clock-enable fix of its own; this one, calling
 * USART_InitAsync() directly with no driver layer above it, does.)
 *
 * Like this vendor's GPIO adapter, USART_InitAsync() does not enable
 * its own peripheral clock. hf_uart_init() now enables the decoded
 * instance's USART clock unconditionally before USART_InitAsync() —
 * idempotent, same reasoning as the GPIO fix, and identical to the
 * fix in hal_port_silabs_uart.c since this is the same classic USART
 * hardware.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
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
#endif /* TARGET_SILABS_SLGPIO */
