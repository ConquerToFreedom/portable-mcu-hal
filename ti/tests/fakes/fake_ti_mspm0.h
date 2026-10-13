/* Test-control surface for tests/fakes/ti_mspm0's fake TI driverlib.
 * Tests call these to script behavior and inspect what the adapter
 * under test did. Adapters under test never include this file - only
 * "ti_msp_dl_config.h", exactly like a real SysConfig project.
 */
#ifndef HAL_WRAPPER_FAKE_TI_MSPM0_H
#define HAL_WRAPPER_FAKE_TI_MSPM0_H

#include "ti_msp_dl_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void fake_ti_reset(void);

/* ---- GPIO ------------------------------------------------------------ */
void fake_ti_gpio_set_input(GPIO_Regs *port, uint32_t mask, bool state);

/* ---- UART --------------------------------------------------------------- */
/* DL_UART_Main_isTXFIFOFull()'s next return value (default false). */
extern bool fake_ti_uart_tx_fifo_full;
/* DL_UART_Main_isRXFIFOEmpty()'s next return value (default true). */
extern bool fake_ti_uart_rx_fifo_empty;
/* DL_UART_Main_receiveData()'s next return value. */
extern uint8_t fake_ti_uart_next_rx_byte;
/* DL_UART_Main_getPendingInterrupt()'s next return value - set this,
 * then call the adapter's own UARTx_IRQHandler() to simulate that
 * interrupt firing (default DL_UART_MAIN_IIDX_NONE). */
extern DL_UART_MAIN_IIDX fake_ti_uart_pending_interrupt;
/* Last byte(s) DL_UART_Main_transmitData() was given. */
extern uint8_t fake_ti_uart_last_tx_byte;

/* ---- I2C ------------------------------------------------------------------ */
/* DL_I2C_getControllerStatus()'s next return value (default: IDLE set,
 * no error, RX FIFO not empty - so both the idle-spin and the
 * read-fifo-drain spin terminate immediately). */
extern uint32_t fake_ti_i2c_status;
extern uint8_t fake_ti_i2c_next_rx_byte;
extern DL_I2C_IIDX fake_ti_i2c_pending_interrupt;

/* ---- SPI -------------------------------------------------------------------- */
extern uint8_t fake_ti_spi_next_rx_byte;
extern DL_SPI_IIDX fake_ti_spi_pending_interrupt;

/* ---- ADC ---------------------------------------------------------------------- */
/* DL_ADC12_getMemResult()'s next return value. */
extern uint16_t fake_ti_adc_next_value;
/* DL_ADC12_getPendingInterrupt()'s next return value (default
 * DL_ADC12_IIDX_MEM0_RESULT_LOADED - always "ready", same reasoning as
 * this project's other vendors' blocking ADC spin-waits). */
extern DL_ADC12_IIDX fake_ti_adc_pending_interrupt;

/* ---- Timer (DL_TimerG) --------------------------------------------------------- */
/* DL_Timer_getPendingInterrupt()'s next return value - set this, then
 * call the adapter's own TIMGx_IRQHandler() to simulate that interrupt
 * firing (default DL_TIMER_IIDX_NONE). */
extern DL_TIMER_IIDX fake_ti_timer_pending_interrupt;

/* ---- PWM (DL_TimerA) ------------------------------------------------------------ */
/* Read back the compare value hf_pwm_set_duty() last wrote for the
 * given timer instance + channel index, without needing access to the
 * adapter's own static state. */
uint32_t fake_ti_get_cc_value(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX ccIndex);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_TI_MSPM0_H */
