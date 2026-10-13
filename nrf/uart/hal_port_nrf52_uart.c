/* nRF52 (nrfx) vendor adapter for the UART portion of hal_wrapper.h,
 * against nrfx_uarte (UART master with EasyDMA). Same nrfx-is-shared-
 * between-nRF5-SDK-and-nRF-Connect-SDK reasoning as
 * nrf/gpio/hal_port_nrf52.c applies here too.
 *
 * Same pin situation as nrf/i2c/hal_port_nrf52_i2c.c: nRF52
 * peripherals wire pins directly in their own init call — EDIT
 * s_uart_pins[] below to match your board's actual TX/RX wiring.
 *
 * Verified nrfx_uarte.h directly before writing this — one real
 * surprise: UARTE's continuous-reception model is buffer-chained, not
 * a simple "one byte, one interrupt" scheme. nrfx_uarte_rx_enable()
 * with NRFX_UARTE_RX_ENABLE_CONT starts continuous reception, but the
 * driver asks for the *next* receive buffer ahead of time via an
 * NRFX_UARTE_EVT_RX_BUF_REQUEST event, and only hands back completed
 * data via NRFX_UARTE_EVT_RX_DONE (carrying p_buffer + length) once a
 * buffer segment fills. This adapter ping-pongs between two 1-byte
 * buffers to get the same "notify per byte" granularity the other
 * adapters use — simple and correct, at the cost of one interrupt per
 * byte rather than per larger chunk; a larger chunk size would cut
 * interrupt overhead at the cost of this file's simplicity, and is a
 * reasonable future optimization, not a correctness requirement.
 *
 * Because every real UARTE operation goes through the registered
 * event handler (there's no way to get a NULL-handler blocking mode
 * here while also running the background RX path the async tier and
 * hf_uart_read() both need), hf_uart_write() itself is implemented as
 * "kick off nrfx_uarte_tx(), then spin-wait on this file's own busy
 * flag" — same pattern as hf_i2c_write_read on this vendor's I2C
 * adapter. An `is_async` flag distinguishes a plain blocking write
 * from a real hf_uart_write_async() call, so the registered TX
 * callback only fires for the latter — otherwise a plain
 * hf_uart_write() would spuriously trigger a callback the caller
 * never asked for from this path.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_UART

#include <stdint.h>
#include "nrfx_uarte.h"

static nrfx_uarte_t s_uarte_instances[] = {
    NRFX_UARTE_INSTANCE(0),
    NRFX_UARTE_INSTANCE(1),
};
#define HF_NRF52_UART_COUNT (sizeof(s_uarte_instances) / sizeof(s_uarte_instances[0]))

/* EDIT ME: TX/RX pin numbers for each UARTE instance — placeholders,
 * not verified board defaults. */
static const struct { uint32_t tx; uint32_t rx; } s_uart_pins[HF_NRF52_UART_COUNT] = {
    { .tx = 6, .rx = 8 },   /* UARTE0 — EDIT ME */
    { .tx = 26, .rx = 27 }, /* UARTE1 — EDIT ME */
};

#define HF_UART_RX_BUFFER_SIZE 128

typedef struct {
    uint8_t buffer[HF_UART_RX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} hf_uart_rx_ring_t;

static hf_uart_rx_ring_t s_rx_ring[HF_NRF52_UART_COUNT];
static uint8_t s_rx_chunk[HF_NRF52_UART_COUNT][2]; /* ping-pong 1-byte chunks */
static uint8_t s_rx_chunk_next[HF_NRF52_UART_COUNT];
static volatile bool s_tx_busy[HF_NRF52_UART_COUNT];
static bool s_tx_is_async[HF_NRF52_UART_COUNT];
static hf_uart_tx_callback_t s_tx_callback[HF_NRF52_UART_COUNT];
static hf_uart_rx_callback_t s_rx_callback[HF_NRF52_UART_COUNT];

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

static nrfx_uarte_t *hf_decode_uart(hf_uart_t instance)
{
    if (instance >= HF_NRF52_UART_COUNT) {
        return NULL;
    }
    return &s_uarte_instances[instance];
}

static nrf_uarte_baudrate_t hf_uart_baud_to_nrf(uint32_t baud_rate)
{
    if (baud_rate >= 921600) return NRF_UARTE_BAUDRATE_921600;
    if (baud_rate >= 460800) return NRF_UARTE_BAUDRATE_460800;
    if (baud_rate >= 230400) return NRF_UARTE_BAUDRATE_230400;
    if (baud_rate >= 115200) return NRF_UARTE_BAUDRATE_115200;
    if (baud_rate >= 57600)  return NRF_UARTE_BAUDRATE_57600;
    if (baud_rate >= 38400)  return NRF_UARTE_BAUDRATE_38400;
    if (baud_rate >= 19200)  return NRF_UARTE_BAUDRATE_19200;
    return NRF_UARTE_BAUDRATE_9600;
}

static void hf_uart_event_handler(nrfx_uarte_event_t const *p_event, void *p_context)
{
    hf_uart_t instance = (hf_uart_t)(uintptr_t)p_context;

    switch (p_event->type) {
    case NRFX_UARTE_EVT_TX_DONE:
        s_tx_busy[instance] = false;
        if (s_tx_is_async[instance]) {
            s_tx_is_async[instance] = false;
            if (s_tx_callback[instance] != NULL) {
                s_tx_callback[instance](instance);
            }
        }
        break;
    case NRFX_UARTE_EVT_RX_DONE: {
        size_t i;
        for (i = 0; i < p_event->data.rx.length; i++) {
            hf_uart_rx_ring_push(&s_rx_ring[instance], p_event->data.rx.p_buffer[i]);
        }
        if (s_rx_callback[instance] != NULL) {
            s_rx_callback[instance](instance);
        }
        break;
    }
    case NRFX_UARTE_EVT_RX_BUF_REQUEST: {
        uint8_t next = s_rx_chunk_next[instance];
        s_rx_chunk_next[instance] = next ^ 1U;
        (void)nrfx_uarte_rx_buffer_set(&s_uarte_instances[instance],
                                        &s_rx_chunk[instance][next], 1);
        break;
    }
    case NRFX_UARTE_EVT_ERROR:
        /* hf_uart_tx_callback_t carries no success/failure signal
         * (deliberately — see docs/architecture.md); clear the busy
         * flag so a failed transfer doesn't permanently block future
         * hf_uart_write_async() calls, but don't invoke the callback,
         * since doing so would incorrectly imply success. */
        s_tx_busy[instance] = false;
        s_tx_is_async[instance] = false;
        break;
    default:
        break;
    }
}

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate)
{
    nrfx_uarte_t *uarte = hf_decode_uart(instance);
    if (uarte == NULL) {
        return false;
    }

    nrfx_uarte_config_t config = {0};
    config.txd_pin = s_uart_pins[instance].tx;
    config.rxd_pin = s_uart_pins[instance].rx;
    config.rts_pin = NRF_UARTE_PSEL_DISCONNECTED;
    config.cts_pin = NRF_UARTE_PSEL_DISCONNECTED;
    config.baudrate = hf_uart_baud_to_nrf(baud_rate);
    /* config.config left zeroed — matches this wrapper's fixed 8N1/
     * no-flow-control contract (no parity, no hardware flow control). */

    if (nrfx_uarte_init(uarte, &config, hf_uart_event_handler) != NRFX_SUCCESS) {
        return false;
    }

    s_rx_chunk_next[instance] = 1;
    (void)nrfx_uarte_rx_buffer_set(uarte, &s_rx_chunk[instance][0], 1);
    return nrfx_uarte_rx_enable(uarte, NRFX_UARTE_RX_ENABLE_CONT) == NRFX_SUCCESS;
}

void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    nrfx_uarte_t *uarte = hf_decode_uart(instance);
    if (uarte == NULL) {
        return;
    }
    s_tx_is_async[instance] = false;
    s_tx_busy[instance] = true;
    if (nrfx_uarte_tx(uarte, data, len, 0) != NRFX_SUCCESS) {
        s_tx_busy[instance] = false;
        return;
    }
    while (s_tx_busy[instance]) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as the header's own
         * hf_uart_write contract. */
    }
}

uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len)
{
    if (instance >= HF_NRF52_UART_COUNT) {
        return 0;
    }
    return hf_uart_rx_ring_pop(&s_rx_ring[instance], data, len);
}

void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback)
{
    if (instance < HF_NRF52_UART_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback)
{
    if (instance < HF_NRF52_UART_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    nrfx_uarte_t *uarte = hf_decode_uart(instance);
    if (uarte == NULL || s_tx_busy[instance]) {
        return false;
    }
    s_tx_is_async[instance] = true;
    s_tx_busy[instance] = true;
    if (nrfx_uarte_tx(uarte, data, len, 0) != NRFX_SUCCESS) {
        s_tx_busy[instance] = false;
        s_tx_is_async[instance] = false;
        return false;
    }
    return true;
}

#endif /* HF_ENABLE_UART */
#endif /* TARGET_NRF52 */
