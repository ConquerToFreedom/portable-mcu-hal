/* Fake stand-in for Silicon Labs emlib's em_cmu.h, used ONLY to
 * host-compile and unit-test this vendor's classic (TARGET_SILABS) and
 * IADC/sl_sleeptimer (TARGET_SILABS_SLGPIO) adapters that still talk to
 * plain emlib-shaped clock-gated peripherals — GPIO, UART, I2C, ADC,
 * Timer, and the current SDK's IADC. Not a copy of Silicon Labs'
 * header.
 */
#ifndef HAL_WRAPPER_FAKE_EM_CMU_H
#define HAL_WRAPPER_FAKE_EM_CMU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    cmuClock_HFPER,
    cmuClock_GPIO,
    cmuClock_USART0,
    cmuClock_USART1,
    cmuClock_I2C0,
    cmuClock_I2C1,
    cmuClock_ADC0,
    cmuClock_ADC1,
    cmuClock_IADC0,
    cmuClock_TIMER0,
    cmuClock_TIMER1,
    cmuClock_TIMER2,
    cmuClock_TIMER3
} CMU_Clock_TypeDef;

void CMU_ClockEnable(CMU_Clock_TypeDef clock, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_CMU_H */
