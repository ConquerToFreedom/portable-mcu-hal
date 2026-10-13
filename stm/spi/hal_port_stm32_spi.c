/* STM32 HAL vendor adapter for the SPI portion of hal_wrapper.h.
 *
 * Same handle-ownership model as this vendor's I2C/UART adapters:
 * references CubeMX's own hspi1/hspi2/hspi3 externs (trim if your
 * project doesn't enable all three — same link-error-by-design
 * reasoning as the I2C adapter).
 *
 * Your CubeMX SPI configuration must have hardware NSS management
 * disabled (Software NSS, or NSS not used) — this wrapper manages
 * chip-select entirely through the `cs_pin` GPIO argument on every
 * call, and a hardware-driven NSS output would fight that.
 *
 * `clock_hz` is advisory only, same category of limitation as this
 * vendor's I2C `clock_hz`: SPI's BaudRatePrescaler is a simple
 * peripheral-clock divisor, genuinely computable at runtime in
 * principle, but doing so correctly needs knowing which clock domain
 * (PCLK1 vs PCLK2) feeds this specific SPI instance — information
 * that varies by part and isn't safely guessable here. `mode`
 * (CPOL/CPHA) has no such ambiguity and is applied directly by this
 * adapter, the same "touch only the field our own parameter controls,
 * leave the rest as CubeMX configured it" approach the UART adapter
 * uses for baud rate.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_SPI

#include <stddef.h>
#include "main.h"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;

static SPI_HandleTypeDef * const s_spi_handle_table[] = { &hspi1, &hspi2, &hspi3 };
#define HF_STM32_SPI_COUNT (sizeof(s_spi_handle_table) / sizeof(s_spi_handle_table[0]))

static hf_gpio_t s_active_cs[HF_STM32_SPI_COUNT];
static bool s_is_read[HF_STM32_SPI_COUNT];
static volatile bool s_busy[HF_STM32_SPI_COUNT];
static hf_spi_tx_callback_t s_tx_callback[HF_STM32_SPI_COUNT];
static hf_spi_rx_callback_t s_rx_callback[HF_STM32_SPI_COUNT];

static SPI_HandleTypeDef *hf_decode_spi(hf_spi_t instance)
{
    if (instance >= HF_STM32_SPI_COUNT) {
        return NULL;
    }
    return s_spi_handle_table[instance];
}

static int hf_spi_index_of(SPI_HandleTypeDef *hspi)
{
    size_t i;
    for (i = 0; i < HF_STM32_SPI_COUNT; i++) {
        if (s_spi_handle_table[i] == hspi) {
            return (int)i;
        }
    }
    return -1;
}

bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL) {
        return false;
    }
    (void)clock_hz; /* advisory only on this vendor — see file header comment */

    switch (mode) {
    case HF_SPI_MODE_0:
        hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
        hspi->Init.CLKPhase = SPI_PHASE_1EDGE;
        break;
    case HF_SPI_MODE_1:
        hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
        hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
        break;
    case HF_SPI_MODE_2:
        hspi->Init.CLKPolarity = SPI_POLARITY_HIGH;
        hspi->Init.CLKPhase = SPI_PHASE_1EDGE;
        break;
    case HF_SPI_MODE_3:
        hspi->Init.CLKPolarity = SPI_POLARITY_HIGH;
        hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
        break;
    default:
        return false;
    }

    return HAL_SPI_Init(hspi) == HAL_OK;
}

bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hspi, (uint8_t *)data, (uint16_t)len, HAL_MAX_DELAY);
    hf_gpio_write(cs_pin, true);
    return status == HAL_OK;
}

bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    HAL_StatusTypeDef status = HAL_SPI_Receive(hspi, data, (uint16_t)len, HAL_MAX_DELAY);
    hf_gpio_write(cs_pin, true);
    return status == HAL_OK;
}

bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(hspi, (uint8_t *)tx_data, rx_data,
                                                         (uint16_t)len, HAL_MAX_DELAY);
    hf_gpio_write(cs_pin, true);
    return status == HAL_OK;
}

void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback)
{
    if (instance < HF_STM32_SPI_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback)
{
    if (instance < HF_STM32_SPI_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL || s_busy[instance]) {
        return false;
    }
    s_busy[instance] = true;
    s_is_read[instance] = false;
    s_active_cs[instance] = cs_pin;
    hf_gpio_write(cs_pin, false);
    if (HAL_SPI_Transmit_IT(hspi, (uint8_t *)data, (uint16_t)len) != HAL_OK) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        return false;
    }
    return true;
}

bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPI_HandleTypeDef *hspi = hf_decode_spi(instance);
    if (hspi == NULL || s_busy[instance]) {
        return false;
    }
    s_busy[instance] = true;
    s_is_read[instance] = true;
    s_active_cs[instance] = cs_pin;
    hf_gpio_write(cs_pin, false);
    if (HAL_SPI_Receive_IT(hspi, data, (uint16_t)len) != HAL_OK) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        return false;
    }
    return true;
}

static void hf_spi_complete(SPI_HandleTypeDef *hspi)
{
    int index = hf_spi_index_of(hspi);
    if (index < 0) {
        return;
    }
    hf_gpio_write(s_active_cs[index], true);
    s_busy[index] = false;
    if (s_is_read[index]) {
        if (s_rx_callback[index] != NULL) {
            s_rx_callback[index]((hf_spi_t)index);
        }
    } else {
        if (s_tx_callback[index] != NULL) {
            s_tx_callback[index]((hf_spi_t)index);
        }
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    hf_spi_complete(hspi);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    hf_spi_complete(hspi);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    int index = hf_spi_index_of(hspi);
    if (index < 0) {
        return;
    }
    hf_gpio_write(s_active_cs[index], true);
    /* hf_spi_tx/rx_callback_t carry no success/failure signal (same
     * reasoning as UART's callbacks) — clear busy without invoking
     * the callback, since firing it would incorrectly imply success. */
    s_busy[index] = false;
}

#endif /* HF_ENABLE_SPI */
#endif /* TARGET_STM32 */
