/* Fake stand-in for a SysConfig-generated "ti_msp_dl_config.h" plus the
 * TI driverlib (dl_gpio.h/dl_uart_main.h/dl_i2c.h/dl_spi.h/dl_adc12.h)
 * headers it pulls in, used ONLY to host-compile and unit-test this
 * vendor's adapters under ti/ against something other than real TI
 * silicon. Not a copy of TI's header - every type, macro and function
 * here exists solely to give the adapters' exact real call shapes
 * (verified against TexasInstruments/mspm0-sdk during this project's
 * earlier research passes) somewhere to link on a host machine.
 */
#ifndef HAL_WRAPPER_FAKE_TI_MSP_DL_CONFIG_H
#define HAL_WRAPPER_FAKE_TI_MSP_DL_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- GPIO ------------------------------------------------------------ */

typedef struct { int _opaque; } GPIO_Regs;

extern GPIO_Regs fake_ti_gpioa;
extern GPIO_Regs fake_ti_gpiob;
#define GPIOA (&fake_ti_gpioa)
#define GPIOB (&fake_ti_gpiob)

void DL_GPIO_enableOutput(GPIO_Regs *port, uint32_t mask);
void DL_GPIO_disableOutput(GPIO_Regs *port, uint32_t mask);
void DL_GPIO_setPins(GPIO_Regs *port, uint32_t mask);
void DL_GPIO_clearPins(GPIO_Regs *port, uint32_t mask);
void DL_GPIO_togglePins(GPIO_Regs *port, uint32_t mask);
uint32_t DL_GPIO_readPins(GPIO_Regs *port, uint32_t mask);

/* ---- UART (DL_UART_Main_*) --------------------------------------------- */

typedef struct { int _opaque; } UART_Regs;

extern UART_Regs fake_ti_uart0;
extern UART_Regs fake_ti_uart1;
#define UART0 (&fake_ti_uart0)
#define UART1 (&fake_ti_uart1)

#define DL_UART_MAIN_INTERRUPT_RX       (1U << 0)
#define DL_UART_MAIN_INTERRUPT_EOT_DONE (1U << 1)

typedef enum { DL_UART_MAIN_IIDX_RX, DL_UART_MAIN_IIDX_EOT_DONE, DL_UART_MAIN_IIDX_NONE } DL_UART_MAIN_IIDX;

void DL_UART_Main_enableInterrupt(UART_Regs *uart, uint32_t flags);
bool DL_UART_Main_isTXFIFOFull(UART_Regs *uart);
void DL_UART_Main_transmitData(UART_Regs *uart, uint8_t data);
DL_UART_MAIN_IIDX DL_UART_Main_getPendingInterrupt(UART_Regs *uart);
bool DL_UART_Main_isRXFIFOEmpty(UART_Regs *uart);
uint8_t DL_UART_Main_receiveData(UART_Regs *uart);

/* ---- I2C (DL_I2C_*) --------------------------------------------------- */

typedef struct { int _opaque; } I2C_Regs;

extern I2C_Regs fake_ti_i2c0;
extern I2C_Regs fake_ti_i2c1;
#define I2C0 (&fake_ti_i2c0)
#define I2C1 (&fake_ti_i2c1)

#define DL_I2C_CONTROLLER_STATUS_IDLE        (1U << 0)
#define DL_I2C_CONTROLLER_STATUS_ERROR       (1U << 1)
#define DL_I2C_CONTROLLER_STATUS_RXFIFO_EMPTY (1U << 2)

typedef enum { DL_I2C_CONTROLLER_DIRECTION_TX, DL_I2C_CONTROLLER_DIRECTION_RX } DL_I2C_CONTROLLER_DIRECTION;
typedef enum { DL_I2C_CONTROLLER_START_DISABLE, DL_I2C_CONTROLLER_START_ENABLE } DL_I2C_CONTROLLER_START;
typedef enum { DL_I2C_CONTROLLER_STOP_DISABLE, DL_I2C_CONTROLLER_STOP_ENABLE } DL_I2C_CONTROLLER_STOP;
typedef enum { DL_I2C_CONTROLLER_ACK_DISABLE, DL_I2C_CONTROLLER_ACK_ENABLE } DL_I2C_CONTROLLER_ACK;

#define DL_I2C_INTERRUPT_CONTROLLER_TX_DONE (1U << 0)
#define DL_I2C_INTERRUPT_CONTROLLER_RX_DONE (1U << 1)

typedef enum { DL_I2C_IIDX_CONTROLLER_TX_DONE, DL_I2C_IIDX_CONTROLLER_RX_DONE, DL_I2C_IIDX_NONE } DL_I2C_IIDX;

uint32_t DL_I2C_getControllerStatus(I2C_Regs *i2c);
void DL_I2C_fillControllerTXFIFO(I2C_Regs *i2c, uint8_t *data, uint32_t len);
void DL_I2C_startControllerTransfer(I2C_Regs *i2c, uint8_t addr, DL_I2C_CONTROLLER_DIRECTION dir, uint32_t len);
void DL_I2C_startControllerTransferAdvanced(I2C_Regs *i2c, uint8_t addr, DL_I2C_CONTROLLER_DIRECTION dir,
                                            uint32_t len, DL_I2C_CONTROLLER_START start,
                                            DL_I2C_CONTROLLER_STOP stop, DL_I2C_CONTROLLER_ACK ack);
uint8_t DL_I2C_receiveControllerData(I2C_Regs *i2c);
void DL_I2C_enableInterrupt(I2C_Regs *i2c, uint32_t flags);
DL_I2C_IIDX DL_I2C_getPendingInterrupt(I2C_Regs *i2c);

/* ---- SPI (DL_SPI_*) ----------------------------------------------------- */

typedef struct { int _opaque; } SPI_Regs;

extern SPI_Regs fake_ti_spi0;
extern SPI_Regs fake_ti_spi1;
#define SPI0 (&fake_ti_spi0)
#define SPI1 (&fake_ti_spi1)

typedef enum {
    DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0, DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA1,
    DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA0, DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA1
} DL_SPI_FRAME_FORMAT;

typedef enum { DL_SPI_MODE_CONTROLLER, DL_SPI_MODE_PERIPHERAL } DL_SPI_MODE;
typedef enum { DL_SPI_PARITY_NONE } DL_SPI_PARITY;
typedef enum { DL_SPI_DATA_SIZE_8 } DL_SPI_DATA_SIZE;
typedef enum { DL_SPI_BIT_ORDER_MSB_FIRST, DL_SPI_BIT_ORDER_LSB_FIRST } DL_SPI_BIT_ORDER;
#define DL_SPI_CHIP_SELECT_NONE 0U

typedef struct {
    DL_SPI_MODE mode;
    DL_SPI_FRAME_FORMAT frameFormat;
    DL_SPI_PARITY parity;
    DL_SPI_DATA_SIZE dataSize;
    DL_SPI_BIT_ORDER bitOrder;
    uint32_t chipSelectPin;
} DL_SPI_Config;

#define DL_SPI_INTERRUPT_RX (1U << 0)
typedef enum { DL_SPI_IIDX_RX, DL_SPI_IIDX_NONE } DL_SPI_IIDX;

void DL_SPI_init(SPI_Regs *spi, DL_SPI_Config *config);
void DL_SPI_enable(SPI_Regs *spi);
void DL_SPI_transmitDataBlocking8(SPI_Regs *spi, uint8_t data);
uint8_t DL_SPI_receiveDataBlocking8(SPI_Regs *spi);
void DL_SPI_enableInterrupt(SPI_Regs *spi, uint32_t flags);
void DL_SPI_disableInterrupt(SPI_Regs *spi, uint32_t flags);
DL_SPI_IIDX DL_SPI_getPendingInterrupt(SPI_Regs *spi);

/* ---- ADC (DL_ADC12_*) --------------------------------------------------- */

typedef struct { int _opaque; } ADC12_Regs;

extern ADC12_Regs fake_ti_adc0;
extern ADC12_Regs fake_ti_adc1;
#define ADC0 (&fake_ti_adc0)
#define ADC1 (&fake_ti_adc1)

typedef enum { DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_REPEAT_MODE_ENABLED } DL_ADC12_REPEAT_MODE;
typedef enum { DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_SAMPLING_SOURCE_MANUAL } DL_ADC12_SAMPLING_SOURCE;
typedef enum { DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_TRIG_SRC_EVENT } DL_ADC12_TRIG_SRC;
typedef enum { DL_ADC12_SAMP_CONV_RES_8_BIT, DL_ADC12_SAMP_CONV_RES_10_BIT, DL_ADC12_SAMP_CONV_RES_12_BIT } DL_ADC12_SAMP_CONV_RES;
typedef enum { DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED, DL_ADC12_SAMP_CONV_DATA_FORMAT_SIGNED } DL_ADC12_SAMP_CONV_DATA_FORMAT;

typedef enum {
    DL_ADC12_INPUT_CHAN_0, DL_ADC12_INPUT_CHAN_1, DL_ADC12_INPUT_CHAN_2, DL_ADC12_INPUT_CHAN_3,
    DL_ADC12_INPUT_CHAN_4, DL_ADC12_INPUT_CHAN_5, DL_ADC12_INPUT_CHAN_6, DL_ADC12_INPUT_CHAN_7,
    DL_ADC12_INPUT_CHAN_8, DL_ADC12_INPUT_CHAN_9, DL_ADC12_INPUT_CHAN_10, DL_ADC12_INPUT_CHAN_11,
    DL_ADC12_INPUT_CHAN_12, DL_ADC12_INPUT_CHAN_13, DL_ADC12_INPUT_CHAN_14, DL_ADC12_INPUT_CHAN_15
} DL_ADC12_INPUT_CHAN;

typedef enum { DL_ADC12_MEM_IDX_0 } DL_ADC12_MEM_IDX;
typedef enum { DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_REFERENCE_VOLTAGE_INTREF } DL_ADC12_REFERENCE_VOLTAGE;
typedef enum { DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0 } DL_ADC12_SAMPLE_TIMER_SOURCE;
typedef enum { DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_AVERAGING_MODE_ENABLED } DL_ADC12_AVERAGING_MODE;
typedef enum { DL_ADC12_BURN_OUT_SOURCE_DISABLED } DL_ADC12_BURN_OUT_SOURCE;
typedef enum { DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_TRIGGER_MODE_AUTO_ONLY } DL_ADC12_TRIGGER_MODE;
typedef enum { DL_ADC12_WINDOWS_COMP_MODE_DISABLED } DL_ADC12_WINDOWS_COMP_MODE;

#define DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED (1U << 0)
/* A #define bitmask, not an enum: hal_port_ti_mspm0_adc.c uses this
 * both as a bitmask (`getPendingInterrupt(adc) & ..._LOADED`, in the
 * blocking spin) and by equality (`== ..._LOADED`, in the IRQ
 * handler) - both hold together only because this is the single bit
 * that's ever set when this interrupt is the only one enabled, same
 * as real driverlib's IIDX register convention. */
#define DL_ADC12_IIDX_MEM0_RESULT_LOADED (1U << 0)
typedef uint32_t DL_ADC12_IIDX;

void DL_ADC12_reset(ADC12_Regs *adc);
void DL_ADC12_enablePower(ADC12_Regs *adc);
void DL_ADC12_setSampleTime0(ADC12_Regs *adc, uint32_t cycles);
void DL_ADC12_initSingleSample(ADC12_Regs *adc, DL_ADC12_REPEAT_MODE repeat, DL_ADC12_SAMPLING_SOURCE source,
                                DL_ADC12_TRIG_SRC trig, DL_ADC12_SAMP_CONV_RES res,
                                DL_ADC12_SAMP_CONV_DATA_FORMAT format);
void DL_ADC12_enableConversions(ADC12_Regs *adc);
void DL_ADC12_configConversionMem(ADC12_Regs *adc, DL_ADC12_MEM_IDX idx, DL_ADC12_INPUT_CHAN chansel,
                                  DL_ADC12_REFERENCE_VOLTAGE vref, DL_ADC12_SAMPLE_TIMER_SOURCE stime,
                                  DL_ADC12_AVERAGING_MODE avgen, DL_ADC12_BURN_OUT_SOURCE bcsen,
                                  DL_ADC12_TRIGGER_MODE trig, DL_ADC12_WINDOWS_COMP_MODE wincomp);
void DL_ADC12_startConversion(ADC12_Regs *adc);
DL_ADC12_IIDX DL_ADC12_getPendingInterrupt(ADC12_Regs *adc);
uint16_t DL_ADC12_getMemResult(ADC12_Regs *adc, DL_ADC12_MEM_IDX idx);
void DL_ADC12_enableInterrupt(ADC12_Regs *adc, uint32_t flags);
void DL_ADC12_disableInterrupt(ADC12_Regs *adc, uint32_t flags);

/* ---- Timer (DL_TimerG) / PWM (DL_TimerA) -------------------------------- */

typedef struct { int _opaque; } GPTIMER_Regs;

extern GPTIMER_Regs fake_ti_timg0;
extern GPTIMER_Regs fake_ti_timg1;
extern GPTIMER_Regs fake_ti_tima0;
extern GPTIMER_Regs fake_ti_tima1;
#define TIMG0 (&fake_ti_timg0)
#define TIMG1 (&fake_ti_timg1)
#define TIMA0 (&fake_ti_tima0)
#define TIMA1 (&fake_ti_tima1)

typedef enum { DL_TIMER_CLOCK_BUSCLK } DL_TIMER_CLOCK;
typedef enum { DL_TIMER_CLOCK_DIVIDE_1 } DL_TIMER_CLOCK_DIVIDE;

typedef struct {
    DL_TIMER_CLOCK clockSel;
    DL_TIMER_CLOCK_DIVIDE divideRatio;
    uint8_t prescale;
} DL_Timer_ClockConfig;

typedef enum { DL_TIMER_TIMER_MODE_ONE_SHOT, DL_TIMER_TIMER_MODE_PERIODIC } DL_TIMER_TIMER_MODE;
typedef enum { DL_TIMER_STOP, DL_TIMER_START } DL_TIMER;
typedef enum { DL_TIMER_INTERM_INT_DISABLED, DL_TIMER_INTERM_INT_ENABLED } DL_TIMER_INTERM_INT;

typedef struct {
    uint32_t period;
    DL_TIMER_TIMER_MODE timerMode;
    DL_TIMER startTimer;
    DL_TIMER_INTERM_INT genIntermInt;
    uint32_t counterVal;
} DL_Timer_TimerConfig;

#define DL_TIMER_INTERRUPT_ZERO (1U << 0)
typedef enum { DL_TIMER_IIDX_ZERO, DL_TIMER_IIDX_NONE } DL_TIMER_IIDX;

void DL_Timer_setClockConfig(GPTIMER_Regs *timer, DL_Timer_ClockConfig const *config);
void DL_Timer_initTimerMode(GPTIMER_Regs *timer, DL_Timer_TimerConfig const *config);
void DL_Timer_enableInterrupt(GPTIMER_Regs *timer, uint32_t flags);
void DL_Timer_startCounter(GPTIMER_Regs *timer);
void DL_Timer_stopCounter(GPTIMER_Regs *timer);
DL_TIMER_IIDX DL_Timer_getPendingInterrupt(GPTIMER_Regs *timer);

typedef enum { DL_TIMER_PWM_MODE_EDGE_ALIGN, DL_TIMER_PWM_MODE_CENTER_ALIGN } DL_TIMER_PWM_MODE;

typedef struct {
    uint32_t period;
    DL_TIMER_PWM_MODE pwmMode;
    bool isTimerWithFourCC;
    DL_TIMER startTimer;
} DL_Timer_PWMConfig;

typedef enum {
    DL_TIMER_CC_0_INDEX, DL_TIMER_CC_1_INDEX, DL_TIMER_CC_2_INDEX, DL_TIMER_CC_3_INDEX
} DL_TIMER_CC_INDEX;

void DL_Timer_initFourCCPWMMode(GPTIMER_Regs *timer, DL_Timer_PWMConfig const *config);
void DL_Timer_setCaptureCompareValue(GPTIMER_Regs *timer, uint32_t value, DL_TIMER_CC_INDEX ccIndex);
uint32_t DL_Timer_getCaptureCompareValue(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX ccIndex);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_TI_MSP_DL_CONFIG_H */
