/* Silicon Labs (emlib) vendor adapter for the I2C portion of
 * hal_wrapper.h — the classic/legacy API, gated by TARGET_SILABS,
 * same split as the GPIO adapters (see hal_port_silabs_slgpio.c for
 * the current sl_i2c driver instead).
 *
 * Verified I2C_TransferSeq_TypeDef's exact field shapes before writing
 * this — worth calling out one detail that's easy to get wrong: emlib's
 * `addr` field wants the 7-bit address already shifted left by one
 * (format "AAAA AAAX", same convention STM32 HAL's DevAddress uses),
 * not the plain 7-bit value hf_i2c_write's device_addr parameter
 * carries.
 *
 * I2C_Transfer(i2c) is emlib's own single pump function — it advances
 * whatever transfer I2C_TransferInit() last started on that peripheral
 * and is meant to be called either from a blocking spin loop or from
 * an interrupt handler; emlib tracks the in-flight sequence internally
 * per instance, so this file just supplies the two call sites and a
 * dispatch table, not a state machine of its own.
 *
 * Same clock-enable bug this vendor's GPIO adapter had: I2C_Init() does
 * not enable the peripheral's own bus clock — hf_i2c_init() now enables
 * the decoded instance's I2C clock unconditionally before I2C_Init(),
 * idempotent same as the GPIO fix.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_I2C

#include "em_i2c.h"
#include "em_cmu.h"

#ifdef I2C0
#define HF_SILABS_I2C_0 I2C0
#else
#define HF_SILABS_I2C_0 NULL
#endif
#ifdef I2C1
#define HF_SILABS_I2C_1 I2C1
#else
#define HF_SILABS_I2C_1 NULL
#endif

static I2C_TypeDef * const s_i2c_table[] = { HF_SILABS_I2C_0, HF_SILABS_I2C_1 };
#define HF_SILABS_I2C_COUNT (sizeof(s_i2c_table) / sizeof(s_i2c_table[0]))

static I2C_TransferSeq_TypeDef s_seq[HF_SILABS_I2C_COUNT];
static bool s_async_pending[HF_SILABS_I2C_COUNT];
static bool s_async_is_read[HF_SILABS_I2C_COUNT];
static hf_i2c_tx_callback_t s_tx_callback[HF_SILABS_I2C_COUNT];
static hf_i2c_rx_callback_t s_rx_callback[HF_SILABS_I2C_COUNT];

static I2C_TypeDef *hf_decode_i2c(hf_i2c_t instance)
{
    if (instance >= HF_SILABS_I2C_COUNT) {
        return NULL;
    }
    return s_i2c_table[instance];
}

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }

#ifdef I2C0
    if (i2c == I2C0) { CMU_ClockEnable(cmuClock_I2C0, true); }
#endif
#ifdef I2C1
    if (i2c == I2C1) { CMU_ClockEnable(cmuClock_I2C1, true); }
#endif

    I2C_Init_TypeDef init = I2C_INIT_DEFAULT;
    init.freq = clock_hz;
    I2C_Init(i2c, &init);

#ifdef I2C0
    if (i2c == I2C0) {
        NVIC_EnableIRQ(I2C0_IRQn);
    }
#endif
#ifdef I2C1
    if (i2c == I2C1) {
        NVIC_EnableIRQ(I2C1_IRQn);
    }
#endif
    return true;
}

static I2C_TransferReturn_TypeDef hf_i2c_run_blocking(I2C_TypeDef *i2c, I2C_TransferSeq_TypeDef *seq)
{
    I2C_TransferReturn_TypeDef ret = I2C_TransferInit(i2c, seq);
    while (ret == i2cTransferInProgress) {
        /* Also fine to call from the ISR while spinning here — emlib's
         * own I2C_Transfer() is idempotent to call redundantly, it
         * just returns i2cTransferInProgress again if nothing's new. */
        ret = I2C_Transfer(i2c);
    }
    return ret;
}

bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    s_seq[instance].addr = (uint16_t)(device_addr << 1);
    s_seq[instance].flags = I2C_FLAG_WRITE;
    s_seq[instance].buf[0].data = (uint8_t *)data;
    s_seq[instance].buf[0].len = (uint16_t)len;
    return hf_i2c_run_blocking(i2c, &s_seq[instance]) == i2cTransferDone;
}

bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    s_seq[instance].addr = (uint16_t)(device_addr << 1);
    s_seq[instance].flags = I2C_FLAG_READ;
    s_seq[instance].buf[0].data = data;
    s_seq[instance].buf[0].len = (uint16_t)len;
    return hf_i2c_run_blocking(i2c, &s_seq[instance]) == i2cTransferDone;
}

bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    /* I2C_FLAG_WRITE_READ is emlib's own combined descriptor — repeated
     * START between the write and read phases, no STOP in between,
     * matching the header's contract without us hand-rolling it. */
    s_seq[instance].addr = (uint16_t)(device_addr << 1);
    s_seq[instance].flags = I2C_FLAG_WRITE_READ;
    s_seq[instance].buf[0].data = (uint8_t *)tx_data;
    s_seq[instance].buf[0].len = (uint16_t)tx_len;
    s_seq[instance].buf[1].data = rx_data;
    s_seq[instance].buf[1].len = (uint16_t)rx_len;
    return hf_i2c_run_blocking(i2c, &s_seq[instance]) == i2cTransferDone;
}

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback)
{
    if (instance < HF_SILABS_I2C_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback)
{
    if (instance < HF_SILABS_I2C_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    s_seq[instance].addr = (uint16_t)(device_addr << 1);
    s_seq[instance].flags = I2C_FLAG_WRITE;
    s_seq[instance].buf[0].data = (uint8_t *)data;
    s_seq[instance].buf[0].len = (uint16_t)len;
    s_async_is_read[instance] = false;
    s_async_pending[instance] = true;
    if (I2C_TransferInit(i2c, &s_seq[instance]) < 0) {
        s_async_pending[instance] = false;
        return false;
    }
    return true;
}

bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_TypeDef *i2c = hf_decode_i2c(instance);
    if (i2c == NULL) {
        return false;
    }
    s_seq[instance].addr = (uint16_t)(device_addr << 1);
    s_seq[instance].flags = I2C_FLAG_READ;
    s_seq[instance].buf[0].data = data;
    s_seq[instance].buf[0].len = (uint16_t)len;
    s_async_is_read[instance] = true;
    s_async_pending[instance] = true;
    if (I2C_TransferInit(i2c, &s_seq[instance]) < 0) {
        s_async_pending[instance] = false;
        return false;
    }
    return true;
}

static void hf_i2c_service_irq(hf_i2c_t instance, I2C_TypeDef *i2c)
{
    I2C_TransferReturn_TypeDef ret = I2C_Transfer(i2c);
    if (ret == i2cTransferInProgress) {
        return;
    }
    if (s_async_pending[instance]) {
        s_async_pending[instance] = false;
        bool success = (ret == i2cTransferDone);
        if (s_async_is_read[instance]) {
            if (s_rx_callback[instance] != NULL) {
                s_rx_callback[instance](instance, success);
            }
        } else {
            if (s_tx_callback[instance] != NULL) {
                s_tx_callback[instance](instance, success);
            }
        }
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
#endif /* TARGET_SILABS */
