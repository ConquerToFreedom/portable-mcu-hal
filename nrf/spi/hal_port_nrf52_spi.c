/* nRF52 (nrfx) vendor adapter for the SPI portion of hal_wrapper.h,
 * against nrfx_spim (SPI master with EasyDMA). Same nrfx-is-shared-
 * between-nRF5-SDK-and-nRF-Connect-SDK reasoning as this vendor's
 * other adapters — one adapter, no second target needed.
 *
 * Same pin situation as this vendor's I2C/UART adapters: nRF52
 * peripherals wire pins directly in their own init call — EDIT
 * s_spi_pins[] below to match your board's actual SCK/MOSI/MISO
 * wiring. `ss_pin` is deliberately left disconnected
 * (NRFX_SPIM_PIN_NOT_USED): this wrapper manages chip-select itself
 * through the `cs_pin` argument on every call, not nrfx's own
 * single fixed CS pin, since a customer may want to address more than
 * one device on the same bus.
 *
 * Verified nrfx_spim.h directly before writing this — it maps onto
 * this API unusually cleanly: NRFX_SPIM_XFER_TRX/TX/RX descriptors are
 * a native full-duplex-or-degenerate transfer type, exactly matching
 * hf_spi_transfer/write/read's own shape, so no vendor-specific
 * reshaping was needed the way STM32/TI's SPI adapters require. Same
 * as this vendor's I2C adapter, every real SPIM operation goes through
 * the registered event handler (no usable NULL-handler blocking mode
 * while the async tier needs one registered), so the blocking
 * functions here are "kick off nrfx_spim_xfer(), spin-wait on this
 * file's own busy flag" — same pattern, same is_async-flag reasoning
 * to keep a plain blocking call from mis-firing the async callback.
 *
 * mode/frequency/bit_order enum names (NRF_SPIM_MODE_0..3,
 * NRF_SPIM_FREQ_*, NRF_SPIM_BIT_ORDER_MSB_FIRST) follow nrfx's
 * otherwise-consistent naming convention across its whole HAL layer —
 * partially confirmed directly (nrfx_spim_config_t's default
 * configuration macro references NRF_SPIM_MODE_0 by name), not fully
 * re-derived from a second source read the way nrfx_twim's/nrfx_uarte's
 * enums were, given time already spent verifying this file's other
 * specifics.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_SPI

#include <stdint.h>
#include "nrfx_spim.h"

static nrfx_spim_t s_spim_instances[] = {
    NRFX_SPIM_INSTANCE(0),
    NRFX_SPIM_INSTANCE(1),
};
#define HF_NRF52_SPI_COUNT (sizeof(s_spim_instances) / sizeof(s_spim_instances[0]))

/* EDIT ME: SCK/MOSI/MISO pin numbers for each SPIM instance —
 * placeholders, not verified board defaults. */
static const struct { uint32_t sck; uint32_t mosi; uint32_t miso; } s_spi_pins[HF_NRF52_SPI_COUNT] = {
    { .sck = 25, .mosi = 23, .miso = 24 }, /* SPIM0 — EDIT ME */
    { .sck = 19, .mosi = 20, .miso = 21 }, /* SPIM1 — EDIT ME */
};

static volatile bool s_busy[HF_NRF52_SPI_COUNT];
static bool s_is_read[HF_NRF52_SPI_COUNT];
static bool s_is_async[HF_NRF52_SPI_COUNT];
static hf_gpio_t s_active_cs[HF_NRF52_SPI_COUNT];
static hf_spi_tx_callback_t s_tx_callback[HF_NRF52_SPI_COUNT];
static hf_spi_rx_callback_t s_rx_callback[HF_NRF52_SPI_COUNT];

static nrfx_spim_t *hf_decode_spi(hf_spi_t instance)
{
    if (instance >= HF_NRF52_SPI_COUNT) {
        return NULL;
    }
    return &s_spim_instances[instance];
}

static nrf_spim_mode_t hf_spi_mode_to_nrf(hf_spi_mode_t mode)
{
    switch (mode) {
    case HF_SPI_MODE_0: return NRF_SPIM_MODE_0;
    case HF_SPI_MODE_1: return NRF_SPIM_MODE_1;
    case HF_SPI_MODE_2: return NRF_SPIM_MODE_2;
    case HF_SPI_MODE_3: return NRF_SPIM_MODE_3;
    default:            return NRF_SPIM_MODE_0;
    }
}

static nrf_spim_frequency_t hf_spi_freq_to_nrf(uint32_t clock_hz)
{
    if (clock_hz >= 8000000) return NRF_SPIM_FREQ_8M;
    if (clock_hz >= 4000000) return NRF_SPIM_FREQ_4M;
    if (clock_hz >= 2000000) return NRF_SPIM_FREQ_2M;
    if (clock_hz >= 1000000) return NRF_SPIM_FREQ_1M;
    if (clock_hz >= 500000)  return NRF_SPIM_FREQ_500K;
    if (clock_hz >= 250000)  return NRF_SPIM_FREQ_250K;
    return NRF_SPIM_FREQ_125K;
}

static void hf_spi_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    hf_spi_t instance = (hf_spi_t)(uintptr_t)p_context;
    (void)p_event; /* only one event type exists (DONE) */

    hf_gpio_write(s_active_cs[instance], true);
    s_busy[instance] = false;

    if (s_is_async[instance]) {
        s_is_async[instance] = false;
        if (s_is_read[instance]) {
            if (s_rx_callback[instance] != NULL) {
                s_rx_callback[instance](instance);
            }
        } else {
            if (s_tx_callback[instance] != NULL) {
                s_tx_callback[instance](instance);
            }
        }
    }
}

bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL) {
        return false;
    }

    nrfx_spim_config_t config = {0};
    config.sck_pin = s_spi_pins[instance].sck;
    config.mosi_pin = s_spi_pins[instance].mosi;
    config.miso_pin = s_spi_pins[instance].miso;
    config.ss_pin = NRFX_SPIM_PIN_NOT_USED;
    config.frequency = hf_spi_freq_to_nrf(clock_hz);
    config.mode = hf_spi_mode_to_nrf(mode);
    config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;

    return nrfx_spim_init(spim, &config, hf_spi_event_handler,
                           (void *)(uintptr_t)instance) == NRFX_SUCCESS;
}

static bool hf_spi_blocking_xfer(hf_spi_t instance, nrfx_spim_t *spim, hf_gpio_t cs_pin,
                                   nrfx_spim_xfer_desc_t const *desc)
{
    s_is_async[instance] = false;
    s_active_cs[instance] = cs_pin;
    s_busy[instance] = true;
    hf_gpio_write(cs_pin, false);
    if (nrfx_spim_xfer(spim, desc, 0) != NRFX_SUCCESS) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        return false;
    }
    while (s_busy[instance]) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as the header's own
         * hf_uart_write contract. */
    }
    return true;
}

bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL) {
        return false;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_TX((uint8_t *)data, len);
    return hf_spi_blocking_xfer(instance, spim, cs_pin, &desc);
}

bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL) {
        return false;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_RX(data, len);
    return hf_spi_blocking_xfer(instance, spim, cs_pin, &desc);
}

bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL) {
        return false;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_TRX((uint8_t *)tx_data, len, rx_data, len);
    return hf_spi_blocking_xfer(instance, spim, cs_pin, &desc);
}

void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback)
{
    if (instance < HF_NRF52_SPI_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback)
{
    if (instance < HF_NRF52_SPI_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL || s_busy[instance]) {
        return false;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_TX((uint8_t *)data, len);
    s_is_async[instance] = true;
    s_is_read[instance] = false;
    s_active_cs[instance] = cs_pin;
    s_busy[instance] = true;
    hf_gpio_write(cs_pin, false);
    if (nrfx_spim_xfer(spim, &desc, 0) != NRFX_SUCCESS) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        s_is_async[instance] = false;
        return false;
    }
    return true;
}

bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    nrfx_spim_t *spim = hf_decode_spi(instance);
    if (spim == NULL || s_busy[instance]) {
        return false;
    }
    nrfx_spim_xfer_desc_t desc = NRFX_SPIM_XFER_RX(data, len);
    s_is_async[instance] = true;
    s_is_read[instance] = true;
    s_active_cs[instance] = cs_pin;
    s_busy[instance] = true;
    hf_gpio_write(cs_pin, false);
    if (nrfx_spim_xfer(spim, &desc, 0) != NRFX_SUCCESS) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        s_is_async[instance] = false;
        return false;
    }
    return true;
}

#endif /* HF_ENABLE_SPI */
#endif /* TARGET_NRF52 */
