/* Test-control surface for tests/fakes/stm32's fake STM32 HAL. Tests
 * call these to script the fake's behavior and inspect what the
 * adapter under test did; the adapter itself never includes this file
 * (it only ever sees main.h, exactly like a real CubeMX project).
 */
#ifndef HAL_WRAPPER_FAKE_STM32_HAL_H
#define HAL_WRAPPER_FAKE_STM32_HAL_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resets every fake counter/script/state variable to its default
 * (HAL_OK everywhere, all GPIO bits low, all buffers cleared). Call
 * this from Unity's setUp() so every test starts from a clean slate. */
void fake_stm32_reset(void);

/* Every blocking/kick-off HAL_* call in this fake returns this value
 * unless a more specific override below applies. Defaults to HAL_OK. */
extern HAL_StatusTypeDef fake_stm32_next_status;

/* HAL_ADC_GetValue()'s next return value. */
extern uint32_t fake_stm32_adc_next_value;

/* Number of times a GPIO port's __HAL_RCC_GPIOx_CLK_ENABLE() macro
 * fired, indexed 0=A .. 3=D. */
extern int fake_stm32_rcc_enable_count[4];

/* Directly force a GPIO input pin's read-back state (IDR), independent
 * of whatever HAL_GPIO_WritePin last drove — simulates an externally
 * driven input pin. port_index: 0=A .. 3=D. */
void fake_stm32_gpio_set_input(int port_index, int pin, bool state);

/* Last data handed to HAL_UART_Transmit/HAL_SPI_Transmit-family calls,
 * and its length, for assertions on what the adapter sent. */
extern const uint8_t *fake_stm32_last_tx_data;
extern uint16_t fake_stm32_last_tx_len;

/* Buffer HAL_UART_Receive_IT was last armed with — write a byte into
 * *fake_stm32_last_rx_buf, then call HAL_UART_RxCpltCallback(), to
 * simulate a byte arriving on the wire. */
extern uint8_t *fake_stm32_last_rx_buf;

/* Overrides HAL_I2C_GetState()'s return value (default
 * HAL_I2C_STATE_READY set by fake_stm32_reset()). */
void fake_stm32_set_i2c_state(HAL_I2C_StateTypeDef state);

/* Storage for the handles the adapters under test extern - same
 * handles a real CubeMX-generated main.c would define. Declared here
 * (not in main.h) so tests can reference &huart1 etc. directly to
 * simulate an ISR firing on a specific instance. */
extern UART_HandleTypeDef huart1, huart2, huart3;
extern I2C_HandleTypeDef hi2c1, hi2c2, hi2c3;
extern SPI_HandleTypeDef hspi1, hspi2, hspi3;
extern ADC_HandleTypeDef hadc1, hadc2, hadc3;
extern TIM_HandleTypeDef htim2, htim3, htim6, htim7;

/* Flash — HAL_FLASHEx_Erase/HAL_FLASH_Program are never real memory
 * operations here (same as every other fake HAL_* call in this file);
 * these just record what the adapter last asked for, and how many
 * times, for assertions. */
extern uint32_t fake_stm32_last_erase_sector;
extern uint32_t fake_stm32_last_erase_nb_sectors;
extern int fake_stm32_erase_call_count;
extern uint32_t fake_stm32_last_program_address;
extern uint64_t fake_stm32_last_program_data;
extern int fake_stm32_program_call_count;
extern int fake_stm32_flash_lock_count;
extern int fake_stm32_flash_unlock_count;

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_STM32_HAL_H */
