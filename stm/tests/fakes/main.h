/* Fake stand-in for a CubeMX-generated "main.h" plus the STM32 HAL
 * headers it pulls in, used ONLY to host-compile and unit-test this
 * vendor's adapters under stm/ against something other than real ST
 * silicon. This is NOT a copy of any ST header - every type, macro and
 * function here exists solely to give the adapters' exact real call
 * shapes (verified against ST's own STM32Cube HAL during this
 * project's earlier research passes) somewhere to link on a host
 * machine. See tests/README.md for why this exists and its limits.
 */
#ifndef HAL_WRAPPER_FAKE_STM32_MAIN_H
#define HAL_WRAPPER_FAKE_STM32_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_OK      = 0x00,
    HAL_ERROR   = 0x01,
    HAL_BUSY    = 0x02,
    HAL_TIMEOUT = 0x03
} HAL_StatusTypeDef;

#define HAL_MAX_DELAY 0xFFFFFFFFU

/* ---- GPIO ---------------------------------------------------------- */

typedef struct { int _opaque; } GPIO_TypeDef;

#define GPIO_MODE_INPUT        0x00000000U
#define GPIO_MODE_OUTPUT_PP    0x00000001U
#define GPIO_MODE_OUTPUT_OD    0x00000011U
#define GPIO_NOPULL            0x00000000U
#define GPIO_PULLUP            0x00000001U
#define GPIO_PULLDOWN          0x00000002U
#define GPIO_SPEED_FREQ_LOW    0x00000000U

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

/* Four ports defined — enough to exercise both "valid port" and
 * "beyond this part's port table" adapter paths; real parts define up
 * to GPIOH. Real vendor headers define GPIOx as a macro to a fixed
 * register address; here it points at a fake storage object instead. */
extern GPIO_TypeDef fake_stm32_gpioa_storage;
extern GPIO_TypeDef fake_stm32_gpiob_storage;
extern GPIO_TypeDef fake_stm32_gpioc_storage;
extern GPIO_TypeDef fake_stm32_gpiod_storage;
#define GPIOA (&fake_stm32_gpioa_storage)
#define GPIOB (&fake_stm32_gpiob_storage)
#define GPIOC (&fake_stm32_gpioc_storage)
#define GPIOD (&fake_stm32_gpiod_storage)

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin);

#define __HAL_RCC_GPIOA_CLK_ENABLE() fake_stm32_rcc_clk_enable(0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() fake_stm32_rcc_clk_enable(1)
#define __HAL_RCC_GPIOC_CLK_ENABLE() fake_stm32_rcc_clk_enable(2)
#define __HAL_RCC_GPIOD_CLK_ENABLE() fake_stm32_rcc_clk_enable(3)
void fake_stm32_rcc_clk_enable(int port_index);

/* ---- UART ------------------------------------------------------------ */

typedef struct {
    uint32_t BaudRate;
} UART_InitTypeDef;

typedef struct {
    UART_InitTypeDef Init;
} UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);

/* Defined BY the adapter under test (weak-override pattern) — declared
 * here only so the fake's completion helpers can call them. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

/* ---- I2C --------------------------------------------------------------- */

typedef struct { int _opaque; } I2C_HandleTypeDef;

typedef enum {
    HAL_I2C_STATE_READY = 0x20,
    HAL_I2C_STATE_BUSY  = 0x24
} HAL_I2C_StateTypeDef;

#define HAL_I2C_ERROR_NONE 0x00000000U
#define HAL_I2C_ERROR_AF   0x00000004U

#define I2C_FIRST_FRAME 0x00000001U
#define I2C_LAST_FRAME  0x00000002U

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef HAL_I2C_Master_Seq_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t opts);
HAL_StatusTypeDef HAL_I2C_Master_Seq_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t addr, uint8_t *data, uint16_t len, uint32_t opts);
HAL_I2C_StateTypeDef HAL_I2C_GetState(I2C_HandleTypeDef *hi2c);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *hi2c);

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);

/* ---- SPI --------------------------------------------------------------- */

typedef struct {
    uint32_t CLKPolarity;
    uint32_t CLKPhase;
} SPI_InitTypeDef;

typedef struct {
    SPI_InitTypeDef Init;
} SPI_HandleTypeDef;

#define SPI_POLARITY_LOW  0x00000000U
#define SPI_POLARITY_HIGH 0x00000001U
#define SPI_PHASE_1EDGE   0x00000000U
#define SPI_PHASE_2EDGE   0x00000001U

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len);
HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t len);

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);

/* ---- ADC ----------------------------------------------------------- */

typedef struct { int _opaque; } ADC_HandleTypeDef;

typedef struct {
    uint32_t Channel;
    uint32_t Rank;
    uint32_t SamplingTime;
} ADC_ChannelConfTypeDef;

#define ADC_SAMPLETIME_3CYCLES 0x00000000U

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *config);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop_IT(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);

/* ---- Timer / PWM (shared TIM_HandleTypeDef) --------------------------- */

typedef struct {
    uint32_t Period;
} TIM_Base_InitTypeDef;

typedef struct {
    TIM_Base_InitTypeDef Init;
    uint32_t CCR[4]; /* fake-only: backs __HAL_TIM_SET_COMPARE below */
} TIM_HandleTypeDef;

#define TIM_CHANNEL_1 0x00000000U
#define TIM_CHANNEL_2 0x00000004U
#define TIM_CHANNEL_3 0x00000008U
#define TIM_CHANNEL_4 0x0000000CU

#define TIM_OCMODE_PWM1     0x00000060U
#define TIM_OCPOLARITY_HIGH 0x00000000U
#define TIM_OCFAST_DISABLE  0x00000000U

typedef struct {
    uint32_t OCMode;
    uint32_t Pulse;
    uint32_t OCPolarity;
    uint32_t OCFastMode;
} TIM_OC_InitTypeDef;

HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *htim);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim);
HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *htim);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *htim, TIM_OC_InitTypeDef *sConfig, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel);

/* Real HAL writes htim->Instance->CCRx directly; this fake's
 * TIM_HandleTypeDef carries a CCR[] array itself instead of a separate
 * register-block pointer, so the macro just indexes it. */
#define __HAL_TIM_SET_COMPARE(htim, channel, value) ((htim)->CCR[(channel) / 4U] = (value))

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_STM32_MAIN_H */
