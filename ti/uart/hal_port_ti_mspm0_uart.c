/* TI MSPM0 (driverlib) vendor adapter for the UART portion of
 * hal_wrapper.h.
 *
 * Assumes the customer's SysConfig-generated "ti_msp_dl_config.h" is
 * on the include path, same assumption as this vendor's GPIO/I2C
 * adapters. baud_rate is advisory only, same category of limitation
 * as this vendor's I2C clock_hz: TI's docs domain and community forum
 * are both blocked in this environment (same access problem as the
 * I2C adapter), so rather than guess a possibly-wrong runtime baud-
 * divisor formula, this file assumes SysConfig's generated init
 * already configured the correct divisor for the baud rate selected
 * in its GUI.
 *
 * TI verification gap, same honesty policy as
 * ti/i2c/hal_port_ti_mspm0_i2c.c: this file uses the `DL_UART_Main_*`
 * function family (confirmed to exist via web search against official
 * TI driverlib doc page titles and a working community example's
 * described usage), but FOUR specific names below rely on driverlib's
 * established, consistent naming pattern rather than a confirmed
 * source read, and should be checked against your installed MSPM0-SDK
 * version's dl_uart_main.h before relying on this in production:
 *   1. DL_UART_Main_isTXFIFOFull — the TX-FIFO-has-room status flag
 *      polled in hf_uart_write().
 *   2. DL_UART_MAIN_IIDX_RX / DL_UART_MAIN_IIDX_EOT_DONE — the
 *      interrupt-index constants checked in the IRQ handler.
 *   3. DL_UART_MAIN_INTERRUPT_RX / DL_UART_MAIN_INTERRUPT_EOT_DONE —
 *      the corresponding enable-interrupt flags.
 * One community example found during research used a *different*,
 * non-"_Main_"-suffixed function family (DL_UART_receiveData,
 * DL_UART_isRXFIFOEmpty) for a UART instance on a different MSPM0
 * part — MSPM0 has both "UART Main" (full-featured) and "UART
 * Extend" (simpler) peripheral variants with separate driverlib APIs.
 * If your part/instance uses the Extend variant, the function names
 * in this file need the "_Main_" removed to match.
 *
 * hf_uart_write_async() fills the entire TX FIFO up front and waits
 * for end-of-transmission — only correct as long as `len` fits within
 * the hardware FIFO depth, same scoping caveat as this vendor's I2C
 * hf_i2c_read_async().
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_UART

#include "ti_msp_dl_config.h"

#ifdef UART0
#define HF_MSPM0_UART_0 UART0
#else
#define HF_MSPM0_UART_0 NULL
#endif
#ifdef UART1
#define HF_MSPM0_UART_1 UART1
#else
#define HF_MSPM0_UART_1 NULL
#endif

static UART_Regs * const s_uart_table[] = { HF_MSPM0_UART_0, HF_MSPM0_UART_1 };
#define HF_MSPM0_UART_COUNT (sizeof(s_uart_table) / sizeof(s_uart_table[0]))

#define HF_UART_RX_BUFFER_SIZE 128

typedef struct {
    uint8_t buffer[HF_UART_RX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} hf_uart_rx_ring_t;

static hf_uart_rx_ring_t s_rx_ring[HF_MSPM0_UART_COUNT];
static volatile bool s_tx_busy[HF_MSPM0_UART_COUNT];
static hf_uart_tx_callback_t s_tx_callback[HF_MSPM0_UART_COUNT];
static hf_uart_rx_callback_t s_rx_callback[HF_MSPM0_UART_COUNT];

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

static UART_Regs *hf_decode_uart(hf_uart_t instance)
{
    if (instance >= HF_MSPM0_UART_COUNT) {
        return NULL;
    }
    return s_uart_table[instance];
}

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate)
{
    UART_Regs *uart = hf_decode_uart(instance);
    if (uart == NULL) {
        return false;
    }
    (void)baud_rate; /* advisory only on this vendor — see file header comment */

    /* NOT independently confirmed — see file header comment. */
    DL_UART_Main_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_RX);
    return true;
}

void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    UART_Regs *uart = hf_decode_uart(instance);
    if (uart == NULL) {
        return;
    }
    uint32_t i;
    for (i = 0; i < len; i++) {
        /* NOT independently confirmed — see file header comment. */
        while (DL_UART_Main_isTXFIFOFull(uart)) {
            /* spin — no timeout primitive is available yet
             * (os_port.h isn't designed), same reasoning as the
             * header's own hf_uart_write contract. */
        }
        DL_UART_Main_transmitData(uart, data[i]);
    }
}

uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len)
{
    if (instance >= HF_MSPM0_UART_COUNT) {
        return 0;
    }
    return hf_uart_rx_ring_pop(&s_rx_ring[instance], data, len);
}

void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback)
{
    if (instance < HF_MSPM0_UART_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback)
{
    if (instance < HF_MSPM0_UART_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    UART_Regs *uart = hf_decode_uart(instance);
    if (uart == NULL || s_tx_busy[instance]) {
        return false;
    }
    s_tx_busy[instance] = true;

    uint32_t i;
    for (i = 0; i < len; i++) {
        /* Only correct as long as len fits within the hardware TX
         * FIFO depth — see file header comment. */
        DL_UART_Main_transmitData(uart, data[i]);
    }
    /* NOT independently confirmed — see file header comment. */
    DL_UART_Main_enableInterrupt(uart, DL_UART_MAIN_INTERRUPT_EOT_DONE);
    return true;
}

static void hf_uart_service_irq(hf_uart_t instance, UART_Regs *uart)
{
    /* NOT independently confirmed — see file header comment. */
    switch (DL_UART_Main_getPendingInterrupt(uart)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(uart)) {
            hf_uart_rx_ring_push(&s_rx_ring[instance], DL_UART_Main_receiveData(uart));
        }
        if (s_rx_callback[instance] != NULL) {
            s_rx_callback[instance](instance);
        }
        break;
    case DL_UART_MAIN_IIDX_EOT_DONE:
        s_tx_busy[instance] = false;
        if (s_tx_callback[instance] != NULL) {
            s_tx_callback[instance](instance);
        }
        break;
    default:
        break;
    }
}

#ifdef UART0
void UART0_IRQHandler(void)
{
    hf_uart_service_irq(0, UART0);
}
#endif
#ifdef UART1
void UART1_IRQHandler(void)
{
    hf_uart_service_irq(1, UART1);
}
#endif

#endif /* HF_ENABLE_UART */
#endif /* TARGET_TI_MSPM0 */
