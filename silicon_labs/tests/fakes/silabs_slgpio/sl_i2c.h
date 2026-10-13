/* Fake stand-in for Silicon Labs' current sl_i2c.h (Simplicity SDK's
 * "Peripheral HAL"), used ONLY to host-compile and unit-test this
 * vendor's current I2C adapter under
 * silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c (TARGET_SILABS_SLGPIO).
 * Not a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_SL_I2C_H
#define HAL_WRAPPER_FAKE_SL_I2C_H

#include <stdint.h>
#include "sl_status.h"
#include "sl_gpio.h"
#include "em_i2c.h" /* for I2C_TypeDef / I2C0 / I2C1 - same hardware block sl_i2c drives */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SL_I2C_LEADER_MODE, SL_I2C_FOLLOWER_MODE } sl_i2c_operating_mode_t;
typedef enum { SL_I2C_FREQ_STANDARD_MODE, SL_I2C_FREQ_FAST_MODE, SL_I2C_FREQ_FASTPLUS_MODE } sl_i2c_freq_mode_t;

typedef struct {
    I2C_TypeDef *i2c_base_addr;
    sl_i2c_operating_mode_t operating_mode;
    uint16_t follower_address;
    sl_gpio_t scl_gpio;
    sl_gpio_t sda_gpio;
    void *fifo_threshold;
    sl_i2c_freq_mode_t freq_mode;
} sl_i2c_init_params_t;

typedef struct sl_i2c_instance_s { int _opaque; } *sl_i2c_handle_t;

typedef enum {
    SL_I2C_EVENT_IN_PROGRESS,
    SL_I2C_EVENT_COMPLETED,
    SL_I2C_EVENT_NACK_RECEIVED,
    SL_I2C_EVENT_ARBITRATION_LOST,
    SL_I2C_EVENT_BUS_ERROR,
    SL_I2C_EVENT_SW_FAULT,
    SL_I2C_EVENT_INVALID_ADDR,
    SL_I2C_EVENT_TIMEOUT,
    SL_I2C_EVENT_STOP,
    SL_I2C_EVENT_IDLE
} sl_i2c_event_t;

typedef sl_status_t (*sl_i2c_callback_t)(sl_i2c_event_t event, void *context);

sl_status_t sl_i2c_init(sl_i2c_init_params_t const *params, sl_i2c_handle_t *handle);
sl_status_t sl_i2c_set_follower_address(sl_i2c_handle_t handle, uint8_t address);
sl_status_t sl_i2c_send_blocking(sl_i2c_handle_t handle, uint8_t const *data, uint16_t len, uint32_t timeout_ms);
sl_status_t sl_i2c_receive_blocking(sl_i2c_handle_t handle, uint8_t *data, uint16_t len, uint32_t timeout_ms);
sl_status_t sl_i2c_transfer(sl_i2c_handle_t handle, uint8_t const *tx_data, uint16_t tx_len,
                            uint8_t *rx_data, uint16_t rx_len);
sl_status_t sl_i2c_send_non_blocking(sl_i2c_handle_t handle, uint8_t const *data, uint16_t len,
                                     sl_i2c_callback_t callback, void *context);
sl_status_t sl_i2c_receive_non_blocking(sl_i2c_handle_t handle, uint8_t *data, uint16_t len,
                                        sl_i2c_callback_t callback, void *context);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SL_I2C_H */
