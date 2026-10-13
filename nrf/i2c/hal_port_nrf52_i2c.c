/* nRF52 (nrfx) vendor adapter for the I2C portion of hal_wrapper.h,
 * against nrfx_twim (TWI master with EasyDMA). Same nrfx-is-shared-
 * between-nRF5-SDK-and-nRF-Connect-SDK reasoning as
 * nrf/gpio/hal_port_nrf52.c applies here too — one adapter, no second
 * target needed.
 *
 * Real difference from STM32/TI: nRF52 peripherals wire SCL/SDA pins
 * directly in their own init call (PSEL registers) — there is no
 * separate "IOMUX pre-muxing" step a config tool does invisibly ahead
 * of time. Someone has to supply pin numbers, so unlike GPIO/UART
 * where "pins are already muxed externally" was enough, I2C on this
 * vendor needs an explicit customization point: EDIT s_twim_pins[]
 * below to match your board's actual SCL/SDA wiring before use. This
 * is a real, permanent asymmetry with STM32/TI, not a placeholder to
 * "finish later".
 *
 * Verified nrfx_twim_xfer_desc_t / nrfx_twim_event_t / function
 * signatures directly against NordicSemiconductor/nrfx's
 * drivers/include/nrfx_twim.h before writing this file. EasyDMA means
 * there is no separate interrupt-vs-DMA choice to make here the way
 * STM32 has — every transfer already goes through EasyDMA; "blocking"
 * vs. "async" is purely about whether this file spin-waits for
 * completion or lets the caller's registered callback do so.
 *
 * hf_i2c_write_read uses NRFX_TWIM_XFER_DESC_TXRX, which is nrfx's own
 * combined write-then-read descriptor type — it holds off the STOP
 * condition between the two phases and issues a repeated START,
 * exactly matching the header's contract. Per nrfx's own
 * documentation, TXRX transfers are only supported in non-blocking
 * (event-handler-registered) mode, which is why every function here —
 * including the "blocking" ones — goes through one non-blocking
 * nrfx_twim_xfer() call and then spin-waits on a flag our own event
 * handler sets, rather than relying on nrfx's separate NULL-handler
 * blocking mode.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_I2C

#include <stdint.h>
#include "nrfx_twim.h"

static nrfx_twim_t s_twim_instances[] = {
    NRFX_TWIM_INSTANCE(0),
    NRFX_TWIM_INSTANCE(1),
};
#define HF_NRF52_TWIM_COUNT (sizeof(s_twim_instances) / sizeof(s_twim_instances[0]))

/* EDIT ME: SCL/SDA pin numbers for each TWIM instance — see file
 * header comment. These are placeholders, not verified board
 * defaults; every project needs to set these to its own wiring. */
static const struct { uint32_t scl; uint32_t sda; } s_twim_pins[HF_NRF52_TWIM_COUNT] = {
    { .scl = 27, .sda = 26 }, /* TWIM0 — EDIT ME */
    { .scl = 28, .sda = 29 }, /* TWIM1 — EDIT ME */
};

static volatile bool s_xfer_done[HF_NRF52_TWIM_COUNT];
static volatile bool s_xfer_success[HF_NRF52_TWIM_COUNT];
static bool s_async_pending[HF_NRF52_TWIM_COUNT];
static hf_i2c_tx_callback_t s_tx_callback[HF_NRF52_TWIM_COUNT];
static hf_i2c_rx_callback_t s_rx_callback[HF_NRF52_TWIM_COUNT];

static nrfx_twim_t *hf_decode_twim(hf_i2c_t instance)
{
    if (instance >= HF_NRF52_TWIM_COUNT) {
        return NULL;
    }
    return &s_twim_instances[instance];
}

static void hf_twim_event_handler(nrfx_twim_event_t const *p_event, void *p_context)
{
    hf_i2c_t instance = (hf_i2c_t)(uintptr_t)p_context;
    bool success = (p_event->type == NRFX_TWIM_EVT_DONE);

    if (s_async_pending[instance]) {
        s_async_pending[instance] = false;
        if (p_event->xfer_desc.type == NRFX_TWIM_XFER_RX) {
            if (s_rx_callback[instance] != NULL) {
                s_rx_callback[instance](instance, success);
            }
        } else {
            if (s_tx_callback[instance] != NULL) {
                s_tx_callback[instance](instance, success);
            }
        }
    } else {
        s_xfer_success[instance] = success;
        s_xfer_done[instance] = true;
    }
}

static bool hf_twim_blocking_xfer(hf_i2c_t instance, nrfx_twim_t *twim,
                                   nrfx_twim_xfer_desc_t const *desc)
{
    s_xfer_done[instance] = false;
    s_async_pending[instance] = false;
    if (nrfx_twim_xfer(twim, desc, 0) != NRFX_SUCCESS) {
        return false;
    }
    while (!s_xfer_done[instance]) {
        /* spin — no timeout primitive is available yet (os_port.h
         * isn't designed), same reasoning as UART's blocking write */
    }
    return s_xfer_success[instance];
}

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }

    nrfx_twim_config_t config = {0};
    config.scl = s_twim_pins[instance].scl;
    config.sda = s_twim_pins[instance].sda;
    config.frequency = (clock_hz >= 400000) ? NRF_TWIM_FREQ_400K : NRF_TWIM_FREQ_100K;

    if (nrfx_twim_init(twim, &config, hf_twim_event_handler,
                        (void *)(uintptr_t)instance) != NRFX_SUCCESS) {
        return false;
    }
    nrfx_twim_enable(twim);
    return true;
}

bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }
    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TX(device_addr, (uint8_t *)data, len);
    return hf_twim_blocking_xfer(instance, twim, &desc);
}

bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }
    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_RX(device_addr, data, len);
    return hf_twim_blocking_xfer(instance, twim, &desc);
}

bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }
    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TXRX(device_addr,
                                                            (uint8_t *)tx_data, tx_len,
                                                            rx_data, rx_len);
    return hf_twim_blocking_xfer(instance, twim, &desc);
}

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback)
{
    if (instance < HF_NRF52_TWIM_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback)
{
    if (instance < HF_NRF52_TWIM_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }
    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TX(device_addr, (uint8_t *)data, len);
    s_async_pending[instance] = true;
    if (nrfx_twim_xfer(twim, &desc, 0) != NRFX_SUCCESS) {
        s_async_pending[instance] = false;
        return false;
    }
    return true;
}

bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    nrfx_twim_t *twim = hf_decode_twim(instance);
    if (twim == NULL) {
        return false;
    }
    nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_RX(device_addr, data, len);
    s_async_pending[instance] = true;
    if (nrfx_twim_xfer(twim, &desc, 0) != NRFX_SUCCESS) {
        s_async_pending[instance] = false;
        return false;
    }
    return true;
}

#endif /* HF_ENABLE_I2C */
#endif /* TARGET_NRF52 */
