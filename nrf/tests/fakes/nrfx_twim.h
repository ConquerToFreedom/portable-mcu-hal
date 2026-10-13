/* Fake stand-in for nrfx's nrfx_twim.h, used ONLY to host-compile and
 * unit-test this vendor's I2C adapter under nrf/i2c/. Not a copy of
 * Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_TWIM_H
#define HAL_WRAPPER_FAKE_NRFX_TWIM_H

#include <stdint.h>
#include <stddef.h>

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t drv_inst_idx; } nrfx_twim_t;
#define NRFX_TWIM_INSTANCE(id) { .drv_inst_idx = (id) }

typedef enum { NRF_TWIM_FREQ_100K, NRF_TWIM_FREQ_400K } nrf_twim_frequency_t;

typedef struct {
    uint32_t scl;
    uint32_t sda;
    nrf_twim_frequency_t frequency;
} nrfx_twim_config_t;

typedef enum { NRFX_TWIM_XFER_TX, NRFX_TWIM_XFER_RX, NRFX_TWIM_XFER_TXRX, NRFX_TWIM_XFER_TXTX } nrfx_twim_xfer_type_t;

typedef struct {
    nrfx_twim_xfer_type_t type;
    uint16_t address;
    uint8_t *p_primary_buf;
    size_t primary_length;
    uint8_t *p_secondary_buf;
    size_t secondary_length;
} nrfx_twim_xfer_desc_t;

#define NRFX_TWIM_XFER_DESC_TX(addr, p_data, len) \
    { .type = NRFX_TWIM_XFER_TX, .address = (addr), .p_primary_buf = (p_data), .primary_length = (len), \
      .p_secondary_buf = NULL, .secondary_length = 0 }
#define NRFX_TWIM_XFER_DESC_RX(addr, p_data, len) \
    { .type = NRFX_TWIM_XFER_RX, .address = (addr), .p_primary_buf = (p_data), .primary_length = (len), \
      .p_secondary_buf = NULL, .secondary_length = 0 }
#define NRFX_TWIM_XFER_DESC_TXRX(addr, p_tx, tx_len, p_rx, rx_len) \
    { .type = NRFX_TWIM_XFER_TXRX, .address = (addr), .p_primary_buf = (p_tx), .primary_length = (tx_len), \
      .p_secondary_buf = (p_rx), .secondary_length = (rx_len) }

typedef enum { NRFX_TWIM_EVT_DONE, NRFX_TWIM_EVT_ADDRESS_NACK, NRFX_TWIM_EVT_DATA_NACK } nrfx_twim_evt_type_t;

typedef struct {
    nrfx_twim_evt_type_t type;
    nrfx_twim_xfer_desc_t xfer_desc;
} nrfx_twim_event_t;

typedef void (*nrfx_twim_event_handler_t)(nrfx_twim_event_t const *p_event, void *p_context);

nrfx_err_t nrfx_twim_init(nrfx_twim_t const *p_instance, nrfx_twim_config_t const *p_config,
                          nrfx_twim_event_handler_t handler, void *p_context);
void nrfx_twim_enable(nrfx_twim_t const *p_instance);
nrfx_err_t nrfx_twim_xfer(nrfx_twim_t const *p_instance, nrfx_twim_xfer_desc_t const *p_xfer_desc, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_TWIM_H */
