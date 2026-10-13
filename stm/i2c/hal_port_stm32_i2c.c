/* STM32 HAL vendor adapter for the I2C portion of hal_wrapper.h.
 *
 * Assumes the customer's CubeMX project has I2C1/I2C2/I2C3 (whichever
 * they intend to use) enabled with NVIC interrupts turned on, which
 * generates the standard hi2c1/hi2c2/hi2c3 handles and
 * I2Cx_EV_IRQHandler/I2Cx_ER_IRQHandler stubs in stm32xxx_it.c that
 * call HAL_I2C_EV_IRQHandler(&hi2cN)/HAL_I2C_ER_IRQHandler(&hi2cN).
 * This adapter does not call HAL_I2C_Init() itself — CubeMX's own
 * MX_I2Cx_Init() already configures the peripheral's clock timing
 * before main() runs, so hf_i2c_init()'s clock_hz parameter is
 * advisory only on this vendor: STM32's newer I2C peripheral
 * (F4/F7/G0/G4/L4/H7 and later) configures speed via a single
 * TIMINGR register whose value depends on the clock tree and
 * rise/fall-time assumptions — it isn't a simple Hz-to-register
 * formula, it's what CubeMX's own timing-calculation tool computes
 * for you. Verified this distinction (vs. the older CCR/TRISE-based
 * I2C on F1/F2/F3) via ST's community forum before writing this file,
 * rather than guessing a runtime approximation that could silently
 * violate I2C bus timing on real hardware.
 *
 * Caveat if your project doesn't enable all three I2C instances: this
 * file references hi2c1/hi2c2/hi2c3 unconditionally. There's no
 * portable way to detect "did CubeMX generate this handle" at compile
 * time (project configuration, unlike GPIOA/GPIOB's silicon-presence
 * macros) — if your CubeMX project only enables a subset, trim
 * s_i2c_handle_table[] and the extern declarations above it down to
 * match. An extern reference to a handle CubeMX didn't generate is a
 * link error, by design (loud, not silent) — not a bug in this file.
 *
 * Async tier: overrides HAL's weak HAL_I2C_MasterTxCpltCallback /
 * HAL_I2C_MasterRxCpltCallback / HAL_I2C_ErrorCallback to dispatch to
 * whichever hf_i2c_tx/rx_callback_t was registered for the instance
 * that fired. Don't also define these three functions elsewhere in
 * the same project — only one definition can exist per project.
 *
 * hf_i2c_write_read uses HAL's Sequential transfer API
 * (HAL_I2C_Master_Seq_Transmit_IT / Seq_Receive_IT with
 * I2C_FIRST_FRAME / I2C_LAST_FRAME) specifically so the write and read
 * phases share one START condition with a repeated START between them
 * — not a full STOP+START — matching what the header contract
 * promises and what stricter I2C devices actually require.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_I2C

#include <stddef.h>
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;

static I2C_HandleTypeDef * const s_i2c_handle_table[] = { &hi2c1, &hi2c2, &hi2c3 };
#define HF_STM32_I2C_COUNT (sizeof(s_i2c_handle_table) / sizeof(s_i2c_handle_table[0]))

static hf_i2c_tx_callback_t s_tx_callback[HF_STM32_I2C_COUNT];
static hf_i2c_rx_callback_t s_rx_callback[HF_STM32_I2C_COUNT];

static I2C_HandleTypeDef *hf_decode_i2c(hf_i2c_t instance)
{
    if (instance >= HF_STM32_I2C_COUNT) {
        return NULL;
    }
    return s_i2c_handle_table[instance];
}

static int hf_i2c_index_of(I2C_HandleTypeDef *hi2c)
{
    size_t i;
    for (i = 0; i < HF_STM32_I2C_COUNT; i++) {
        if (s_i2c_handle_table[i] == hi2c) {
            return (int)i;
        }
    }
    return -1;
}

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz)
{
    (void)clock_hz; /* advisory only on this vendor — see file header comment */
    return hf_decode_i2c(instance) != NULL;
}

bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_HandleTypeDef *hi2c = hf_decode_i2c(instance);
    if (hi2c == NULL) {
        return false;
    }
    return HAL_I2C_Master_Transmit(hi2c, (uint16_t)(device_addr << 1),
                                    (uint8_t *)data, (uint16_t)len,
                                    HAL_MAX_DELAY) == HAL_OK;
}

bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_HandleTypeDef *hi2c = hf_decode_i2c(instance);
    if (hi2c == NULL) {
        return false;
    }
    return HAL_I2C_Master_Receive(hi2c, (uint16_t)(device_addr << 1),
                                   data, (uint16_t)len,
                                   HAL_MAX_DELAY) == HAL_OK;
}

bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len)
{
    I2C_HandleTypeDef *hi2c = hf_decode_i2c(instance);
    if (hi2c == NULL) {
        return false;
    }

    if (HAL_I2C_Master_Seq_Transmit_IT(hi2c, (uint16_t)(device_addr << 1),
                                        (uint8_t *)tx_data, (uint16_t)tx_len,
                                        I2C_FIRST_FRAME) != HAL_OK) {
        return false;
    }
    while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as UART's blocking write */
    }
    if (HAL_I2C_GetError(hi2c) != HAL_I2C_ERROR_NONE) {
        return false;
    }

    if (HAL_I2C_Master_Seq_Receive_IT(hi2c, (uint16_t)(device_addr << 1),
                                       rx_data, (uint16_t)rx_len,
                                       I2C_LAST_FRAME) != HAL_OK) {
        return false;
    }
    while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY) {
        /* spin, same as above */
    }
    return HAL_I2C_GetError(hi2c) == HAL_I2C_ERROR_NONE;
}

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback)
{
    if (instance < HF_STM32_I2C_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback)
{
    if (instance < HF_STM32_I2C_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    I2C_HandleTypeDef *hi2c = hf_decode_i2c(instance);
    if (hi2c == NULL || HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY) {
        return false;
    }
    return HAL_I2C_Master_Transmit_IT(hi2c, (uint16_t)(device_addr << 1),
                                       (uint8_t *)data, (uint16_t)len) == HAL_OK;
}

bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    I2C_HandleTypeDef *hi2c = hf_decode_i2c(instance);
    if (hi2c == NULL || HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY) {
        return false;
    }
    return HAL_I2C_Master_Receive_IT(hi2c, (uint16_t)(device_addr << 1),
                                      data, (uint16_t)len) == HAL_OK;
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    int index = hf_i2c_index_of(hi2c);
    if (index >= 0 && s_tx_callback[index] != NULL) {
        s_tx_callback[index]((hf_i2c_t)index, true);
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    int index = hf_i2c_index_of(hi2c);
    if (index >= 0 && s_rx_callback[index] != NULL) {
        s_rx_callback[index]((hf_i2c_t)index, true);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    int index = hf_i2c_index_of(hi2c);
    if (index < 0) {
        return;
    }
    /* HAL doesn't tell us here whether the in-flight transfer was a
     * transmit or a receive, so report failure on whichever callback
     * is registered — a customer using only one direction on this
     * instance only ever sees the one that applies to them. */
    if (s_tx_callback[index] != NULL) {
        s_tx_callback[index]((hf_i2c_t)index, false);
    }
    if (s_rx_callback[index] != NULL) {
        s_rx_callback[index]((hf_i2c_t)index, false);
    }
}

#endif /* HF_ENABLE_I2C */
#endif /* TARGET_STM32 */
