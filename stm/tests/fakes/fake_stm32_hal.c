/* Fake STM32 HAL implementation - backs main.h's declarations with
 * host-runnable bodies so this vendor's adapters under stm/ can be
 * unit tested without real ST silicon. Not a copy of ST's HAL source:
 * every function here is a minimal, scriptable stand-in, not a
 * functional reimplementation of the real peripheral.
 */
#include "fake_stm32_hal.h"
#include <string.h>

HAL_StatusTypeDef fake_stm32_next_status = HAL_OK;
uint32_t fake_stm32_adc_next_value = 0;
int fake_stm32_rcc_enable_count[4];
const uint8_t *fake_stm32_last_tx_data;
uint16_t fake_stm32_last_tx_len;
uint8_t *fake_stm32_last_rx_buf;

GPIO_TypeDef fake_stm32_gpioa_storage;
GPIO_TypeDef fake_stm32_gpiob_storage;
GPIO_TypeDef fake_stm32_gpioc_storage;
GPIO_TypeDef fake_stm32_gpiod_storage;

/* CubeMX-generated handles — a real project defines these in its own
 * main.c; the adapter only externs them. Our fakes provide the
 * storage instead. */
UART_HandleTypeDef huart1, huart2, huart3;
I2C_HandleTypeDef hi2c1, hi2c2, hi2c3;
SPI_HandleTypeDef hspi1, hspi2, hspi3;
ADC_HandleTypeDef hadc1, hadc2, hadc3;
TIM_HandleTypeDef htim2, htim3, htim6, htim7;

static uint16_t s_gpio_odr[4];
static uint16_t s_gpio_idr[4];
static HAL_I2C_StateTypeDef s_i2c_state = HAL_I2C_STATE_READY;
static uint32_t s_i2c_error = HAL_I2C_ERROR_NONE;

void fake_stm32_reset(void)
{
    fake_stm32_next_status = HAL_OK;
    fake_stm32_adc_next_value = 0;
    memset(fake_stm32_rcc_enable_count, 0, sizeof(fake_stm32_rcc_enable_count));
    fake_stm32_last_tx_data = NULL;
    fake_stm32_last_tx_len = 0;
    fake_stm32_last_rx_buf = NULL;
    memset(s_gpio_odr, 0, sizeof(s_gpio_odr));
    memset(s_gpio_idr, 0, sizeof(s_gpio_idr));
    s_i2c_state = HAL_I2C_STATE_READY;
    s_i2c_error = HAL_I2C_ERROR_NONE;
    memset(&huart1, 0, sizeof(huart1));
    memset(&huart2, 0, sizeof(huart2));
    memset(&huart3, 0, sizeof(huart3));
    memset(&hi2c1, 0, sizeof(hi2c1));
    memset(&hi2c2, 0, sizeof(hi2c2));
    memset(&hi2c3, 0, sizeof(hi2c3));
    memset(&hspi1, 0, sizeof(hspi1));
    memset(&hspi2, 0, sizeof(hspi2));
    memset(&hspi3, 0, sizeof(hspi3));
    memset(&hadc1, 0, sizeof(hadc1));
    memset(&hadc2, 0, sizeof(hadc2));
    memset(&hadc3, 0, sizeof(hadc3));
    memset(&htim2, 0, sizeof(htim2));
    memset(&htim3, 0, sizeof(htim3));
    memset(&htim6, 0, sizeof(htim6));
    memset(&htim7, 0, sizeof(htim7));
}

static int hf_test_port_index(GPIO_TypeDef *port)
{
    if (port == &fake_stm32_gpioa_storage) return 0;
    if (port == &fake_stm32_gpiob_storage) return 1;
    if (port == &fake_stm32_gpioc_storage) return 2;
    if (port == &fake_stm32_gpiod_storage) return 3;
    return -1;
}

void fake_stm32_gpio_set_input(int port_index, int pin, bool state)
{
    if (port_index < 0 || port_index >= 4 || pin < 0 || pin >= 16) {
        return;
    }
    if (state) {
        s_gpio_idr[port_index] |= (uint16_t)(1U << pin);
    } else {
        s_gpio_idr[port_index] &= (uint16_t)~(1U << pin);
    }
}

void fake_stm32_rcc_clk_enable(int port_index)
{
    if (port_index >= 0 && port_index < 4) {
        fake_stm32_rcc_enable_count[port_index]++;
    }
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
    (void)port;
    (void)init;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    int index = hf_test_port_index(port);
    if (index < 0) {
        return;
    }
    if (state == GPIO_PIN_SET) {
        s_gpio_odr[index] |= pin;
        s_gpio_idr[index] |= pin;
    } else {
        s_gpio_odr[index] = (uint16_t)(s_gpio_odr[index] & ~pin);
        s_gpio_idr[index] = (uint16_t)(s_gpio_idr[index] & ~pin);
    }
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    int index = hf_test_port_index(port);
    if (index < 0) {
        return GPIO_PIN_RESET;
    }
    return (s_gpio_idr[index] & pin) != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin)
{
    int index = hf_test_port_index(port);
    if (index < 0) {
        return;
    }
    s_gpio_odr[index] ^= pin;
    s_gpio_idr[index] ^= pin;
}

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart)
{
    (void)huart;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len, uint32_t timeout)
{
    (void)huart;
    (void)timeout;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    (void)huart;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    (void)huart;
    (void)len;
    fake_stm32_last_rx_buf = data;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t timeout)
{
    (void)hi2c; (void)addr; (void)timeout;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t timeout)
{
    (void)hi2c; (void)addr; (void)data; (void)len; (void)timeout;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len)
{
    (void)hi2c; (void)addr;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len)
{
    (void)hi2c; (void)addr; (void)data; (void)len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Seq_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t opts)
{
    (void)hi2c; (void)addr; (void)opts;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Seq_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t opts)
{
    (void)hi2c; (void)addr; (void)data; (void)len; (void)opts;
    return fake_stm32_next_status;
}

HAL_I2C_StateTypeDef HAL_I2C_GetState(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;
    return s_i2c_state;
}

void fake_stm32_set_i2c_state(HAL_I2C_StateTypeDef state)
{
    s_i2c_state = state;
}

uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;
    return (fake_stm32_next_status == HAL_OK) ? HAL_I2C_ERROR_NONE : HAL_I2C_ERROR_AF;
}

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len, uint32_t timeout)
{
    (void)hspi; (void)timeout;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len, uint32_t timeout)
{
    (void)hspi; (void)data; (void)timeout;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout)
{
    (void)hspi; (void)rx; (void)timeout;
    fake_stm32_last_tx_data = tx;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len)
{
    (void)hspi;
    fake_stm32_last_tx_data = data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len)
{
    (void)hspi; (void)data;
    fake_stm32_last_tx_len = len;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *config)
{
    (void)hadc; (void)config;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_Stop_IT(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t timeout)
{
    (void)hadc; (void)timeout;
    return fake_stm32_next_status;
}

uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return fake_stm32_adc_next_value;
}

HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *htim)
{
    (void)htim;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim)
{
    (void)htim;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim)
{
    (void)htim;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *htim)
{
    (void)htim;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *htim, TIM_OC_InitTypeDef *sConfig, uint32_t Channel)
{
    (void)htim; (void)sConfig; (void)Channel;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    (void)htim; (void)Channel;
    return fake_stm32_next_status;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    (void)htim; (void)Channel;
    return fake_stm32_next_status;
}
