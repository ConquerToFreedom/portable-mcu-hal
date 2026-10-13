/* Fake stand-in for Silicon Labs emlib's em_timer.h, used ONLY to
 * host-compile and unit-test this vendor's classic Timer and PWM
 * adapters under silicon_labs/timer/hal_port_silabs_timer.c and
 * silicon_labs/pwm/hal_port_silabs_pwm.c (TARGET_SILABS). Not a copy
 * of Silicon Labs' header.
 *
 * Also fakes the GPIO->TIMERROUTE/TIMERROUTE_SET pin-routing registers
 * the PWM adapter pokes directly (real emlib puts these on the GPIO
 * peripheral struct, not TIMER, and there is no portable emlib
 * function for this — see that adapter's own header comment), and
 * defines _GPIO_TIMER_ROUTEEN_MASK so the adapter's newer-Series-2
 * routing code path is what gets compiled and tested here.
 */
#ifndef HAL_WRAPPER_FAKE_EM_TIMER_H
#define HAL_WRAPPER_FAKE_EM_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int _opaque; } TIMER_TypeDef;

extern TIMER_TypeDef fake_silabs_timer0_storage;
extern TIMER_TypeDef fake_silabs_timer1_storage;
extern TIMER_TypeDef fake_silabs_timer2_storage;
extern TIMER_TypeDef fake_silabs_timer3_storage;
#define TIMER0 (&fake_silabs_timer0_storage)
#define TIMER1 (&fake_silabs_timer1_storage)
#define TIMER2 (&fake_silabs_timer2_storage)
#define TIMER3 (&fake_silabs_timer3_storage)

typedef enum { timerPrescale1 } TIMER_Prescale_TypeDef;

typedef struct {
    bool enable;
    TIMER_Prescale_TypeDef prescale;
    bool oneShot;
} TIMER_Init_TypeDef;
#define TIMER_INIT_DEFAULT { true, timerPrescale1, false }

#define TIMER0_IRQn 30
#define TIMER1_IRQn 31
void NVIC_EnableIRQ(int irqn);

void TIMER_Init(TIMER_TypeDef *timer, TIMER_Init_TypeDef const *init);
void TIMER_TopSet(TIMER_TypeDef *timer, uint32_t val);
void TIMER_CounterSet(TIMER_TypeDef *timer, uint32_t val);
void TIMER_Enable(TIMER_TypeDef *timer, bool enable);
void TIMER_IntClear(TIMER_TypeDef *timer, uint32_t flags);
void TIMER_IntEnable(TIMER_TypeDef *timer, uint32_t flags);
uint32_t TIMER_IntGet(TIMER_TypeDef *timer);

/* ---- CC (compare/capture) channels, used for PWM mode ----------------- */

typedef enum { timerCCModeOff, timerCCModePWM } TIMER_CCMode_TypeDef;
typedef enum { timerOutputActionNone, timerOutputActionToggle } TIMER_OutputAction_TypeDef;

typedef struct {
    TIMER_CCMode_TypeDef mode;
    TIMER_OutputAction_TypeDef cmoa;
} TIMER_InitCC_TypeDef;
#define TIMER_INITCC_DEFAULT { timerCCModeOff, timerOutputActionNone }

void TIMER_InitCC(TIMER_TypeDef *timer, unsigned int ch, TIMER_InitCC_TypeDef const *init);
void TIMER_CompareSet(TIMER_TypeDef *timer, unsigned int ch, uint32_t val);
void TIMER_CompareBufSet(TIMER_TypeDef *timer, unsigned int ch, uint32_t val);

/* ---- GPIO->TIMERROUTE pin-routing fake (newer Series 2 scheme) -------- */

#define _GPIO_TIMER_ROUTEEN_MASK        1U
#define _GPIO_TIMER_CC0ROUTE_PORT_SHIFT 0U
#define _GPIO_TIMER_CC0ROUTE_PIN_SHIFT  8U
#define _GPIO_TIMER_ROUTEEN_CC0PEN_SHIFT 0U

int fake_silabs_timer_num(TIMER_TypeDef *timer);
#define TIMER_NUM(timer) fake_silabs_timer_num(timer)

typedef struct {
    uint32_t CC0ROUTE;
    uint32_t CC1ROUTE;
    uint32_t CC2ROUTE;
    uint32_t CC3ROUTE;
} GPIO_TIMERROUTE_TypeDef;

/* FAKE LIMITATION: on real Series 2 hardware, writing to a "_SET"
 * register alias has OR semantics (only the written 1-bits actually
 * change in the real underlying register; 0-bits are no-ops) — that's
 * exactly why the adapter's plain `= 1U << x` assignment to this alias
 * is correct there. This fake's ROUTEEN is a plain struct field with a
 * genuine C assignment, so it has ordinary overwrite semantics
 * instead — a second hf_pwm_start() call for a different channel on
 * the same timer will appear (in this fake only) to clear the first
 * channel's enable bit, which would NOT happen on real hardware. No
 * test in this vendor's test suite currently depends on two channels'
 * enable state surviving across separate hf_pwm_start() calls; add
 * one only if this fake is upgraded to track written bits accurately. */
typedef struct {
    uint32_t ROUTEEN;
} GPIO_TIMERROUTE_SET_TypeDef;

#define HAL_WRAPPER_FAKE_GPIO_TIMERROUTE_COUNT 4 /* TIMER0..TIMER3, indexed by real peripheral number */

typedef struct {
    GPIO_TIMERROUTE_TypeDef TIMERROUTE[HAL_WRAPPER_FAKE_GPIO_TIMERROUTE_COUNT];
    GPIO_TIMERROUTE_SET_TypeDef TIMERROUTE_SET[HAL_WRAPPER_FAKE_GPIO_TIMERROUTE_COUNT];
} GPIO_TypeDef_Peripheral;

extern GPIO_TypeDef_Peripheral fake_silabs_gpio_peripheral;
#define GPIO (&fake_silabs_gpio_peripheral)

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_EM_TIMER_H */
