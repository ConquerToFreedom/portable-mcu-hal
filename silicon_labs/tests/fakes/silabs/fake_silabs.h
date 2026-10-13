/* Test-control surface for tests/fakes/silabs's fake classic emlib
 * (TARGET_SILABS). Tests call these to script behavior and inspect
 * what the adapter under test did. Adapters under test never include
 * this file - only their real emlib headers, exactly like a real
 * Simplicity Studio project.
 */
#ifndef HAL_WRAPPER_FAKE_SILABS_H
#define HAL_WRAPPER_FAKE_SILABS_H

#include "em_gpio.h"
#include "em_cmu.h"
#include "em_usart.h"
#include "em_i2c.h"
#include "em_adc.h"
#include "em_timer.h"
#include "spidrv.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void fake_silabs_reset(void);

/* ---- GPIO ------------------------------------------------------------ */
void fake_silabs_gpio_set_input(GPIO_Port_TypeDef port, unsigned int pin, bool state);

/* ---- UART (em_usart, instance 0 = USART0, 1 = USART1) ----------------- */
/* Directly exposes the fake storage so a test can set RXDATA before
 * firing the RX IRQ handler. */
extern USART_TypeDef fake_silabs_usart0;
extern USART_TypeDef fake_silabs_usart1;

/* Overrides what USART_IntGet() reports for the given instance until
 * changed again; USART_IntClear() clears bits out of it, same as real
 * clear-on-write hardware. */
void fake_silabs_usart_set_flags(int instance, uint32_t flags);

/* ---- I2C (em_i2c) ------------------------------------------------------- */
/* What I2C_Transfer() returns on its first call after a transfer
 * starts (default i2cTransferDone). Used for both the blocking path
 * (I2C_Transfer is polled directly) and the async path (I2C_Transfer
 * is called from I2Cx_IRQHandler). */
extern I2C_TransferReturn_TypeDef fake_silabs_i2c_next_result;

/* ---- SPI (spidrv, shared with TARGET_SILABS_SLGPIO) --------------------- */
extern Ecode_t fake_silabs_spi_next_status;
/* When true (default), a successful SPIDRV_MTransmit/MReceive kick-off
 * immediately invokes its callback synchronously with
 * fake_silabs_spi_next_status, matching how SPIDRV's blocking
 * B-suffixed calls already behave in this fake. Set false to control
 * completion manually via fake_silabs_spi_fire_completion(). */
extern bool fake_silabs_spi_auto_complete;
void fake_silabs_spi_fire_completion(SPIDRV_Handle_t handle, Ecode_t status, int items_transferred);

/* ---- ADC (em_adc) --------------------------------------------------------- */
/* ADC_DataSingleGet()'s next return value. */
extern uint32_t fake_silabs_adc_next_value;

/* ---- Timer (em_timer, instance 0/1 = TIMER0/TIMER1) ------------------------ */
/* Overrides what TIMER_IntGet() reports for the given instance until
 * changed again; TIMER_IntClear() clears bits out of it, same as real
 * clear-on-write hardware and the same pattern as
 * fake_silabs_usart_set_flags(). */
void fake_silabs_timer_set_flags(int instance, uint32_t flags);

/* ---- PWM (em_timer CC channels, instance 0/1 = TIMER2/TIMER3) -------------- */
/* Read back the compare value hf_pwm_set_duty() last wrote for the
 * given TIMER instance + CC channel, via TIMER_CompareBufSet(). */
uint32_t fake_silabs_pwm_get_compare(TIMER_TypeDef *timer, unsigned int ch);
/* Read back the port/pin GPIO->TIMERROUTE[n].CCxROUTE was last set to
 * for the given TIMER instance + CC channel. */
uint32_t fake_silabs_pwm_get_route(TIMER_TypeDef *timer, unsigned int ch);
/* Whether GPIO->TIMERROUTE_SET[n].ROUTEEN's bit for the given CC
 * channel was ever set (hf_pwm_start() sets it). */
bool fake_silabs_pwm_route_enabled(TIMER_TypeDef *timer, unsigned int ch);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SILABS_H */
