/* Fake stand-in for nrfx's nrfx_uarte.h, used ONLY to host-compile and
 * unit-test this vendor's UART adapter under nrf/uart/. Not a copy of
 * Nordic's header.
 *
 * NOTE ON A REAL FINDING surfaced while writing this fake: the adapter
 * calls nrfx_uarte_init(uarte, &config, hf_uart_event_handler) with NO
 * context argument, unlike this same vendor's I2C/SPI adapters, which
 * both pass (void *)(uintptr_t)instance explicitly to their init call.
 * hf_uart_event_handler still casts its p_context parameter straight to
 * an instance index, exactly like the I2C/SPI handlers do. This fake
 * therefore always invokes the handler with p_context == NULL (the
 * most honest simulation of what the real 3-argument call actually
 * supplies), which is why tests/test_nrf52_uart.c only exercises
 * UARTE0. Verify against your installed nrfx version whether
 * multi-instance UARTE event attribution actually works before relying
 * on UARTE1 in production - this may be a genuine latent bug in
 * nrf/uart/hal_port_nrf52_uart.c, not a limitation of this fake.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_UARTE_H
#define HAL_WRAPPER_FAKE_NRFX_UARTE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

typedef struct { uint8_t drv_inst_idx; } nrfx_uarte_t;
#define NRFX_UARTE_INSTANCE(id) { .drv_inst_idx = (id) }

typedef enum {
    NRF_UARTE_BAUDRATE_9600, NRF_UARTE_BAUDRATE_19200, NRF_UARTE_BAUDRATE_38400,
    NRF_UARTE_BAUDRATE_57600, NRF_UARTE_BAUDRATE_115200, NRF_UARTE_BAUDRATE_230400,
    NRF_UARTE_BAUDRATE_460800, NRF_UARTE_BAUDRATE_921600
} nrf_uarte_baudrate_t;

#define NRF_UARTE_PSEL_DISCONNECTED 0xFFFFFFFFU

typedef struct { uint32_t parity_hwfc_bits; } nrf_uarte_config_t;

typedef struct {
    uint32_t txd_pin;
    uint32_t rxd_pin;
    uint32_t rts_pin;
    uint32_t cts_pin;
    nrf_uarte_baudrate_t baudrate;
    nrf_uarte_config_t config;
} nrfx_uarte_config_t;

#define NRFX_UARTE_RX_ENABLE_CONT 1U

typedef enum {
    NRFX_UARTE_EVT_TX_DONE,
    NRFX_UARTE_EVT_RX_DONE,
    NRFX_UARTE_EVT_RX_BUF_REQUEST,
    NRFX_UARTE_EVT_ERROR
} nrfx_uarte_evt_type_t;

typedef struct {
    uint8_t const *p_buffer;
    size_t length;
} nrfx_uarte_rx_data_t;

typedef struct {
    nrfx_uarte_evt_type_t type;
    union {
        nrfx_uarte_rx_data_t rx;
    } data;
} nrfx_uarte_event_t;

typedef void (*nrfx_uarte_event_handler_t)(nrfx_uarte_event_t const *p_event, void *p_context);

nrfx_err_t nrfx_uarte_init(nrfx_uarte_t const *p_instance, nrfx_uarte_config_t const *p_config,
                            nrfx_uarte_event_handler_t handler);
nrfx_err_t nrfx_uarte_rx_buffer_set(nrfx_uarte_t const *p_instance, uint8_t *p_data, size_t length);
nrfx_err_t nrfx_uarte_rx_enable(nrfx_uarte_t const *p_instance, uint32_t flags);
nrfx_err_t nrfx_uarte_tx(nrfx_uarte_t const *p_instance, uint8_t const *p_data, size_t length, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_UARTE_H */
