/* STM32 HAL vendor adapter for the GPIO portion of hal_wrapper.h.
 *
 * Assumes the customer's STM32CubeIDE-generated "main.h" is on the
 * include path, which pulls in the correct stm32fXxx_hal.h for their
 * part — this file never selects a specific STM32 family itself.
 *
 * Decode pattern (bitmask vendor) is the one documented in
 * docs/gpio-api-reference.md: HF_PIN_PORT(pin) indexes a const port
 * table, HF_PIN_NUM(pin) becomes the 1U << n bitmask HAL expects.
 *
 * Our identifiers use the "hf_"/"HF_" prefix rather than "hal_"/"HAL_"
 * on purpose: STM32's own SDK is literally called "STM32 HAL", so this
 * file calls its functions (HAL_GPIO_Init, HAL_GPIO_WritePin, etc. —
 * left as-is below, that's their real names) right next to ours. A
 * shared "hal_" prefix on both sides made the two easy to mix up.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_GPIO

#include <stddef.h>
#include "main.h"

#define HF_PIN_PORT(p)   ((p) >> 5)
#define HF_PIN_NUM(p)    ((p) & 0x1F)

#ifdef GPIOA
#define HF_STM32_PORT_A GPIOA
#else
#define HF_STM32_PORT_A NULL
#endif
#ifdef GPIOB
#define HF_STM32_PORT_B GPIOB
#else
#define HF_STM32_PORT_B NULL
#endif
#ifdef GPIOC
#define HF_STM32_PORT_C GPIOC
#else
#define HF_STM32_PORT_C NULL
#endif
#ifdef GPIOD
#define HF_STM32_PORT_D GPIOD
#else
#define HF_STM32_PORT_D NULL
#endif
#ifdef GPIOE
#define HF_STM32_PORT_E GPIOE
#else
#define HF_STM32_PORT_E NULL
#endif
#ifdef GPIOF
#define HF_STM32_PORT_F GPIOF
#else
#define HF_STM32_PORT_F NULL
#endif
#ifdef GPIOG
#define HF_STM32_PORT_G GPIOG
#else
#define HF_STM32_PORT_G NULL
#endif
#ifdef GPIOH
#define HF_STM32_PORT_H GPIOH
#else
#define HF_STM32_PORT_H NULL
#endif

/* Port base lookup — const, lives in flash, indexed by HF_PIN_PORT(pin).
 * Ports absent on the target part resolve to NULL rather than shifting
 * the indices of the ports that follow. */
static GPIO_TypeDef * const s_port_table[] = {
    HF_STM32_PORT_A, HF_STM32_PORT_B, HF_STM32_PORT_C, HF_STM32_PORT_D,
    HF_STM32_PORT_E, HF_STM32_PORT_F, HF_STM32_PORT_G, HF_STM32_PORT_H,
};
#define HF_STM32_PORT_COUNT (sizeof(s_port_table) / sizeof(s_port_table[0]))

static GPIO_TypeDef *hf_decode_port(hf_gpio_t pin)
{
    uint32_t index = HF_PIN_PORT(pin);
    if (index >= HF_STM32_PORT_COUNT) {
        return NULL;
    }
    return s_port_table[index];
}

/* HAL_GPIO_Init() only configures the pin — the AHB peripheral clock for
 * its port must be gated on first or every operation is a no-op against
 * an unclocked peripheral. STM32 HAL has no generic "enable by pointer",
 * so this switches on the resolved port base. */
static void hf_enable_port_clock(GPIO_TypeDef *port)
{
#ifdef GPIOA
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); return; }
#endif
#ifdef GPIOB
    if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); return; }
#endif
#ifdef GPIOC
    if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); return; }
#endif
#ifdef GPIOD
    if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); return; }
#endif
#ifdef GPIOE
    if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); return; }
#endif
#ifdef GPIOF
    if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); return; }
#endif
#ifdef GPIOG
    if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); return; }
#endif
#ifdef GPIOH
    if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); return; }
#endif
}

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode)
{
    GPIO_TypeDef *port = hf_decode_port(pin);
    if (port == NULL) {
        return false;
    }

    GPIO_InitTypeDef init = {0};
    init.Pin = (uint16_t)(1U << HF_PIN_NUM(pin));
    init.Speed = GPIO_SPEED_FREQ_LOW;

    switch (mode) {
    case HF_GPIO_INPUT:
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = GPIO_NOPULL;
        break;
    case HF_GPIO_INPUT_PULLUP:
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = GPIO_PULLUP;
        break;
    case HF_GPIO_INPUT_PULLDOWN:
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = GPIO_PULLDOWN;
        break;
    case HF_GPIO_OUTPUT_PUSHPULL:
        init.Mode = GPIO_MODE_OUTPUT_PP;
        init.Pull = GPIO_NOPULL;
        break;
    case HF_GPIO_OUTPUT_OPENDRAIN:
        init.Mode = GPIO_MODE_OUTPUT_OD;
        init.Pull = GPIO_NOPULL;
        break;
    default:
        return false;
    }

    hf_enable_port_clock(port);
    HAL_GPIO_Init(port, &init);
    return true;
}

void hf_gpio_write(hf_gpio_t pin, bool state)
{
    GPIO_TypeDef *port = hf_decode_port(pin);
    if (port == NULL) {
        return;
    }
    HAL_GPIO_WritePin(port, (uint16_t)(1U << HF_PIN_NUM(pin)),
                       state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool hf_gpio_read(hf_gpio_t pin)
{
    GPIO_TypeDef *port = hf_decode_port(pin);
    if (port == NULL) {
        return false;
    }
    return HAL_GPIO_ReadPin(port, (uint16_t)(1U << HF_PIN_NUM(pin))) == GPIO_PIN_SET;
}

void hf_gpio_toggle(hf_gpio_t pin)
{
    GPIO_TypeDef *port = hf_decode_port(pin);
    if (port == NULL) {
        return;
    }
    HAL_GPIO_TogglePin(port, (uint16_t)(1U << HF_PIN_NUM(pin)));
}

#endif /* HF_ENABLE_GPIO */
#endif /* TARGET_STM32 */
