/* TI MSPM0 (driverlib) vendor adapter for the I2C portion of
 * hal_wrapper.h.
 *
 * Assumes the customer's SysConfig-generated "ti_msp_dl_config.h" is
 * on the include path, same assumption as hal_port_ti_mspm0.c (GPIO).
 *
 * Repeated START for hf_i2c_write_read uses
 * DL_I2C_startControllerTransferAdvanced(), which — unlike the plain
 * DL_I2C_startControllerTransfer() used for hf_i2c_write/read — gives
 * explicit control over the START/STOP/ACK conditions per phase:
 * TX phase requests START but withholds STOP, RX phase requests START
 * again (producing the repeated START, since the bus was never
 * released) and then STOP to end the transaction. Confirmed this
 * function and its START/STOP/ACK enum shape via TI's own driverlib
 * docs and a related MSPM0 repeated-start forum thread before writing
 * this — TI's docs/forum domains are both proxy-blocked in this
 * environment, so verification here was necessarily partial (web
 * search snippets only, no full header source fetch, unlike every
 * other adapter in this repo). Two specific names below are flagged
 * as NOT independently confirmed and should be checked against your
 * installed MSPM0-SDK version's dl_i2c.h before relying on this in
 * production:
 *   1. The RX-FIFO-has-data status flag polled in hf_i2c_read/
 *      hf_i2c_write_read's read phase.
 *   2. The exact DL_I2C_IIDX_CONTROLLER_* constant names checked in
 *      the IRQ handler for the async tier.
 * Both use TI driverlib's otherwise well-established, consistent
 * naming conventions (DL_I2C_CONTROLLER_STATUS_* bitmask flags,
 * DL_I2C_IIDX_* interrupt-index constants, the same pattern GPIO's
 * own DL_GPIO_IIDX_* uses) — the pattern is solid, only the exact
 * suffix for these two specific values is unconfirmed.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_I2C

#include "ti_msp_dl_config.h"

#ifdef I2C0
#define HF_MSPM0_I2C_0 I2C0
#else
#define HF_MSPM0_I2C_0 NULL
#endif
#ifdef I2C1
#define HF_MSPM0_I2C_1 I2C1
#else
#define HF_MSPM0_I2C_1 NULL
#endif

static I2C_Regs * const s_i2c_table[] = { HF_MSPM0_I2C_0, HF_MSPM0_I2C_1 };
#define HF_MSPM0_I2C_COUNT (sizeof(s_i2c_table) / sizeof(s_i2c_table[0]))

static hf_i2c_tx_callback_t s_tx_callback[HF_MSPM0_I2C_COUNT];
static hf_i2c_rx_callback_t s_rx_callback[HF_MSPM0_I2C_COUNT];
static bool s_async_pending[HF_MSPM0_I2C_COUNT];
static bool s_async_is_read[HF_MSPM0_I2C_COUNT];
static uint8_t *s_async_rx_data[HF_MSPM0_I2C_COUNT];
static uint32_t s_async_rx_len[HF_MSPM0_I2C_COUNT];

static I2C_Regs *hf_decode_i2c(hf_i2c_t instance)
{
    if (instance >= HF_MSPM0_I2C_COUNT) {
        return NULL;
    }
    return s_i2c_table[instance];
}

static void hf_i2c_wait_idle(I2C_Regs *i2c)
{
    while (!(DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as UART's blocking write */
    }
}

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz)
{
    (void)clock_hz; /* clock is set by SysConfig-generated init, same
                      * pre-configuration assumption as the GPIO
                      * adapter's pin muxing */
    return hf_decode_i2c(instance) != NULL;
}

bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_Regs *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_I2C_fillControllerTXFIFO(i2c, (uint8_t *)&data[i], 1);
    }
    DL_I2C_startControllerTransfer(i2c, device_addr, DL_I2C_CONTROLLER_DIRECTION_TX, len);
    hf_i2c_wait_idle(i2c);
    return (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_ERROR) == 0;
}

bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_Regs *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    DL_I2C_startControllerTransfer(i2c, device_addr, DL_I2C_CONTROLLER_DIRECTION_RX, len);
    uint32_t i;
    for (i = 0; i < len; i++) {
        /* NOT independently confirmed — see file header comment. */
        while (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_RXFIFO_EMPTY) {
            /* spin */
        }
        data[i] = DL_I2C_receiveControllerData(i2c);
    }
    hf_i2c_wait_idle(i2c);
    return (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_ERROR) == 0;
}

bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len)
{
    I2C_Regs *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }

    uint32_t i;
    for (i = 0; i < tx_len; i++) {
        DL_I2C_fillControllerTXFIFO(i2c, (uint8_t *)&tx_data[i], 1);
    }
    DL_I2C_startControllerTransferAdvanced(i2c, device_addr,
                                            DL_I2C_CONTROLLER_DIRECTION_TX, tx_len,
                                            DL_I2C_CONTROLLER_START_ENABLE,
                                            DL_I2C_CONTROLLER_STOP_DISABLE,
                                            DL_I2C_CONTROLLER_ACK_ENABLE);
    hf_i2c_wait_idle(i2c);
    if (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_ERROR) {
        return false;
    }

    /* START_ENABLE again here, with the bus never having released
     * (STOP was withheld above), is what produces the repeated START
     * rather than a fresh STOP+START. */
    DL_I2C_startControllerTransferAdvanced(i2c, device_addr,
                                            DL_I2C_CONTROLLER_DIRECTION_RX, rx_len,
                                            DL_I2C_CONTROLLER_START_ENABLE,
                                            DL_I2C_CONTROLLER_STOP_ENABLE,
                                            DL_I2C_CONTROLLER_ACK_ENABLE);
    for (i = 0; i < rx_len; i++) {
        while (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_RXFIFO_EMPTY) {
            /* spin */
        }
        rx_data[i] = DL_I2C_receiveControllerData(i2c);
    }
    hf_i2c_wait_idle(i2c);
    return (DL_I2C_getControllerStatus(i2c) & DL_I2C_CONTROLLER_STATUS_ERROR) == 0;
}

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback)
{
    if (instance < HF_MSPM0_I2C_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback)
{
    if (instance < HF_MSPM0_I2C_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_Regs *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_I2C_fillControllerTXFIFO(i2c, (uint8_t *)&data[i], 1);
    }
    s_async_is_read[instance] = false;
    s_async_pending[instance] = true;
    DL_I2C_enableInterrupt(i2c, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);
    DL_I2C_startControllerTransfer(i2c, device_addr, DL_I2C_CONTROLLER_DIRECTION_TX, len);
    return true;
}

bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_Regs *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    s_async_is_read[instance] = true;
    s_async_pending[instance] = true;
    s_async_rx_data[instance] = data;
    s_async_rx_len[instance] = len;
    DL_I2C_enableInterrupt(i2c, DL_I2C_INTERRUPT_CONTROLLER_RX_DONE);
    DL_I2C_startControllerTransfer(i2c, device_addr, DL_I2C_CONTROLLER_DIRECTION_RX, len);
    return true;
}

static void hf_i2c_service_irq(hf_i2c_t instance, I2C_Regs *i2c)
{
    /* NOT independently confirmed — see file header comment. */
    switch (DL_I2C_getPendingInterrupt(i2c)) {
    case DL_I2C_IIDX_CONTROLLER_TX_DONE:
        if (s_async_pending[instance] && !s_async_is_read[instance]) {
            s_async_pending[instance] = false;
            if (s_tx_callback[instance] != NULL) {
                s_tx_callback[instance](instance, true);
            }
        }
        break;
    case DL_I2C_IIDX_CONTROLLER_RX_DONE:
        if (s_async_pending[instance] && s_async_is_read[instance]) {
            /* RX_DONE firing means the requested length is already in
             * the RX FIFO — drain it here rather than across further
             * interrupt firings. Only correct as long as
             * s_async_rx_len[instance] fits within the hardware FIFO
             * depth; a length longer than that needs re-arming this
             * drain per FIFO-full interrupt instead, which this pass
             * doesn't implement (same "common case first" scoping as
             * the rest of this file's flagged gaps). */
            uint32_t i;
            for (i = 0; i < s_async_rx_len[instance]; i++) {
                s_async_rx_data[instance][i] = DL_I2C_receiveControllerData(i2c);
            }
            s_async_pending[instance] = false;
            if (s_rx_callback[instance] != NULL) {
                s_rx_callback[instance](instance, true);
            }
        }
        break;
    default:
        break;
    }
}

#ifdef I2C0
void I2C0_IRQHandler(void)
{
    hf_i2c_service_irq(0, I2C0);
}
#endif
#ifdef I2C1
void I2C1_IRQHandler(void)
{
    hf_i2c_service_irq(1, I2C1);
}
#endif

#endif /* HF_ENABLE_I2C */
#endif /* TARGET_TI_MSPM0 */
