/* Fake stand-in for Silicon Labs' current sl_pwm.h (Simplicity SDK's
 * "Peripheral HAL" PWM driver), used ONLY to host-compile and
 * unit-test this vendor's current PWM adapter under
 * silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c (TARGET_SILABS_SLGPIO).
 * Not a copy of Silicon Labs' header.
 *
 * sl_pwm_instance_t genuinely holds a plain TIMER_TypeDef* (confirmed
 * against the real sl_pwm.h — see that adapter's own header comment),
 * so this fake includes the same classic em_timer.h fake TIMER0-3
 * definitions live in, rather than inventing a second TIMER type.
 */
#ifndef HAL_WRAPPER_FAKE_SL_PWM_H
#define HAL_WRAPPER_FAKE_SL_PWM_H

#include <stdint.h>
#include "sl_status.h"
#include "em_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PWM_ACTIVE_HIGH, PWM_ACTIVE_LOW } sl_pwm_polarity_t;

typedef struct sl_pwm_config {
    int frequency;
    sl_pwm_polarity_t polarity;
} sl_pwm_config_t;

typedef struct sl_pwm_instance {
    TIMER_TypeDef *timer;
    uint8_t channel;
    uint8_t port;
    uint8_t pin;
    uint8_t location;
} sl_pwm_instance_t;

sl_status_t sl_pwm_init(sl_pwm_instance_t *pwm, sl_pwm_config_t *config);
sl_status_t sl_pwm_deinit(sl_pwm_instance_t *pwm);
void sl_pwm_start(sl_pwm_instance_t *pwm);
void sl_pwm_stop(sl_pwm_instance_t *pwm);
void sl_pwm_set_duty_cycle(sl_pwm_instance_t *pwm, uint8_t percent);
uint8_t sl_pwm_get_duty_cycle(sl_pwm_instance_t *pwm);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_SL_PWM_H */
