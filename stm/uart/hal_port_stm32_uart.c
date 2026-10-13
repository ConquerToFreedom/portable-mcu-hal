/* STM32 HAL vendor adapter for the UART portion of hal_wrapper.h.
 *
 * Same handle-ownership model as stm/i2c/hal_port_stm32_i2c.c:
 * references CubeMX's own huart1/huart2/huart3 externs rather than
 * owning private handles, because the async tier needs CubeMX's own
 * generated USARTx_IRQHandler (calling HAL_UART_IRQHandler(&huartN))
 * to reach whichever handle is actually in use. If your project
 * doesn't enable all three USART instances, trim s_uart_handle_table[]
 * and the extern declarations below to match — an extern reference to
 * a handle CubeMX didn't generate is a link error, by design (loud,
 * not silent), not a bug in this file.
 *
 * Unlike I2C's TIMINGR, UART's baud rate genuinely can be recomputed
 * at runtime — HAL_UART_Init() derives the BRR register from
 * huart->Init.BaudRate with a plain division, no clock-tree-dependent
 * lookup table involved. So hf_uart_init() here calls HAL_UART_Init()
 * itself after overwriting BaudRate, unlike hf_i2c_init()'s advisory-
 * only clock_hz. This assumes CubeMX's generated MX_USARTx_Init()
 * already set WordLength/StopBits/Parity/Mode/HwFlowCtl to match this
 * wrapper's fixed 8N1/no-flow-control contract — this file only ever
 * touches BaudRate, never those other fields.
 *
 * RX uses the classic STM32 HAL circular-receive pattern: arm a
 * single-byte HAL_UART_Receive_IT() and re-arm it from inside
 * HAL_UART_RxCpltCallback() every time a byte arrives, pushing each
 * byte into this file's own ring buffer. hf_uart_read() only drains
 * that ring buffer — see the header's own comment on why a background
 * receive path is required regardless of whether the async tier is
 * used.
 *
 * Async tier: overrides HAL's weak HAL_UART_TxCpltCallback /
 * HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback, same as the I2C
 * adapter overrides the I2C equivalents. Don't also define these
 * three functions elsewhere in the same project.
 *
 * One STM32-specific gotcha worth recording: because the background
 * RX_IT arm never stops, HAL_UART_GetState() never settles at
 * HAL_UART_STATE_READY the way HAL_I2C_GetState() does between I2C
 * transfers — it's always at least RX-busy. So "is a TX transfer
 * already in flight" is tracked with this file's own flag rather than
 * by checking HAL's combined state enum.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_UART

#include <stddef.h>
#include "main.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

static UART_HandleTypeDef * const s_uart_handle_table[] = { &huart1, &huart2, &huart3 };
#define HF_STM32_UART_COUNT (sizeof(s_uart_handle_table) / sizeof(s_uart_handle_table[0]))

#define HF_UART_RX_BUFFER_SIZE 128

typedef struct {
    uint8_t buffer[HF_UART_RX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} hf_uart_rx_ring_t;

static hf_uart_rx_ring_t s_rx_ring[HF_STM32_UART_COUNT];
static uint8_t s_rx_byte[HF_STM32_UART_COUNT];
static volatile bool s_tx_busy[HF_STM32_UART_COUNT];
static hf_uart_tx_callback_t s_tx_callback[HF_STM32_UART_COUNT];
static hf_uart_rx_callback_t s_rx_callback[HF_STM32_UART_COUNT];

static void hf_uart_rx_ring_push(hf_uart_rx_ring_t *ring, uint8_t byte)
{
    uint32_t next = (ring->head + 1U) % HF_UART_RX_BUFFER_SIZE;
    if (next != ring->tail) { /* drop the byte on overflow rather than corrupt the ring */
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

static UART_HandleTypeDef *hf_decode_uart(hf_uart_t instance)
{
    if (instance >= HF_STM32_UART_COUNT) {
        return NULL;
    }
    return s_uart_handle_table[instance];
}

static int hf_uart_index_of(UART_HandleTypeDef *huart)
{
    size_t i;
    for (i = 0; i < HF_STM32_UART_COUNT; i++) {
        if (s_uart_handle_table[i] == huart) {
            return (int)i;
        }
    }
    return -1;
}

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate)
{
    UART_HandleTypeDef *huart = hf_decode_uart(instance);
    if (huart == NULL) {
        return false;
    }

    huart->Init.BaudRate = baud_rate;
    if (HAL_UART_Init(huart) != HAL_OK) {
        return false;
    }

    /* Arm the first byte of the continuous background receive. */
    return HAL_UART_Receive_IT(huart, &s_rx_byte[instance], 1) == HAL_OK;
}

void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    UART_HandleTypeDef *huart = hf_decode_uart(instance);
    if (huart == NULL) {
        return;
    }
    /* HAL_UART_Transmit is genuinely, separately blocking — it does
     * not go through HAL_UART_TxCpltCallback at all, so this can
     * never mis-fire a registered async TX callback the way a
     * spin-wait-on-the-IT-path implementation would. */
    (void)HAL_UART_Transmit(huart, (uint8_t *)data, (uint16_t)len, HAL_MAX_DELAY);
}

uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len)
{
    if (instance >= HF_STM32_UART_COUNT) {
        return 0;
    }
    return hf_uart_rx_ring_pop(&s_rx_ring[instance], data, len);
}

void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback)
{
    if (instance < HF_STM32_UART_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback)
{
    if (instance < HF_STM32_UART_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len)
{
    UART_HandleTypeDef *huart = hf_decode_uart(instance);
    if (huart == NULL || s_tx_busy[instance]) {
        return false;
    }
    s_tx_busy[instance] = true;
    if (HAL_UART_Transmit_IT(huart, (uint8_t *)data, (uint16_t)len) != HAL_OK) {
        s_tx_busy[instance] = false;
        return false;
    }
    return true;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    int index = hf_uart_index_of(huart);
    if (index < 0) {
        return;
    }
    s_tx_busy[index] = false;
    if (s_tx_callback[index] != NULL) {
        s_tx_callback[index]((hf_uart_t)index);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    int index = hf_uart_index_of(huart);
    if (index < 0) {
        return;
    }
    hf_uart_rx_ring_push(&s_rx_ring[index], s_rx_byte[index]);
    /* Re-arm immediately so no byte is missed between callback return
     * and the next arrival. */
    (void)HAL_UART_Receive_IT(huart, &s_rx_byte[index], 1);
    if (s_rx_callback[index] != NULL) {
        s_rx_callback[index]((hf_uart_t)index);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    int index = hf_uart_index_of(huart);
    if (index < 0) {
        return;
    }
    /* Re-arm the background receive so one framing/overrun/noise error
     * doesn't permanently stall RX. */
    (void)HAL_UART_Receive_IT(huart, &s_rx_byte[index], 1);

    /* hf_uart_tx_callback_t carries no success/failure signal (unlike
     * I2C's, where a NACK is routine) — deliberately, per
     * docs/architecture.md: error reporting on UART was scoped out of
     * v1. Clear the busy flag so a failed transfer doesn't permanently
     * block future hf_uart_write_async() calls on this instance, but
     * don't invoke the callback — firing it would incorrectly imply
     * the transfer succeeded, which is worse than not calling it. */
    s_tx_busy[index] = false;
}

#endif /* HF_ENABLE_UART */
#endif /* TARGET_STM32 */
