/* Fake stand-in for nrfx's nrfx_spim.h, used ONLY to host-compile and
 * unit-test this vendor's SPI adapter under nrf/spi/. Not a copy of
 * Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_SPIM_H
#define HAL_WRAPPER_FAKE_NRFX_SPIM_H

#include <stdint.h>
#include <stddef.h>

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t drv_inst_idx; } nrfx_spim_t;
#define NRFX_SPIM_INSTANCE(id) { .drv_inst_idx = (id) }

#define NRFX_SPIM_PIN_NOT_USED 0xFFFFFFFFU

typedef enum {
    NRF_SPIM_FREQ_125K, NRF_SPIM_FREQ_250K, NRF_SPIM_FREQ_500K, NRF_SPIM_FREQ_1M,
    NRF_SPIM_FREQ_2M, NRF_SPIM_FREQ_4M, NRF_SPIM_FREQ_8M
} nrf_spim_frequency_t;

typedef enum { NRF_SPIM_MODE_0, NRF_SPIM_MODE_1, NRF_SPIM_MODE_2, NRF_SPIM_MODE_3 } nrf_spim_mode_t;
typedef enum { NRF_SPIM_BIT_ORDER_MSB_FIRST, NRF_SPIM_BIT_ORDER_LSB_FIRST } nrf_spim_bit_order_t;

typedef struct {
    uint32_t sck_pin;
    uint32_t mosi_pin;
    uint32_t miso_pin;
    uint32_t ss_pin;
    nrf_spim_frequency_t frequency;
    nrf_spim_mode_t mode;
    nrf_spim_bit_order_t bit_order;
} nrfx_spim_config_t;

typedef struct {
    uint8_t const *p_tx_buffer;
    size_t tx_length;
    uint8_t *p_rx_buffer;
    size_t rx_length;
} nrfx_spim_xfer_desc_t;

#define NRFX_SPIM_XFER_TX(p_data, len) \
    { .p_tx_buffer = (p_data), .tx_length = (len), .p_rx_buffer = NULL, .rx_length = 0 }
#define NRFX_SPIM_XFER_RX(p_data, len) \
    { .p_tx_buffer = NULL, .tx_length = 0, .p_rx_buffer = (p_data), .rx_length = (len) }
#define NRFX_SPIM_XFER_TRX(p_tx, tx_len, p_rx, rx_len) \
    { .p_tx_buffer = (p_tx), .tx_length = (tx_len), .p_rx_buffer = (p_rx), .rx_length = (rx_len) }

typedef enum { NRFX_SPIM_EVENT_DONE } nrfx_spim_evt_type_t;

typedef struct {
    nrfx_spim_evt_type_t type;
} nrfx_spim_evt_t;

typedef void (*nrfx_spim_event_handler_t)(nrfx_spim_evt_t const *p_event, void *p_context);

nrfx_err_t nrfx_spim_init(nrfx_spim_t const *p_instance, nrfx_spim_config_t const *p_config,
                          nrfx_spim_event_handler_t handler, void *p_context);
nrfx_err_t nrfx_spim_xfer(nrfx_spim_t const *p_instance, nrfx_spim_xfer_desc_t const *p_xfer_desc, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_SPIM_H */
