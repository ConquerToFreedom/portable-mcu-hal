/* Fake TI driverlib implementation - backs ti_msp_dl_config.h's
 * declarations with host-runnable bodies so this vendor's adapters
 * under ti/ can be unit tested without real TI silicon. Not a copy of
 * TI's driverlib source.
 */
#include "fake_ti_mspm0.h"
#include <string.h>

GPIO_Regs fake_ti_gpioa;
GPIO_Regs fake_ti_gpiob;
UART_Regs fake_ti_uart0;
UART_Regs fake_ti_uart1;
I2C_Regs fake_ti_i2c0;
I2C_Regs fake_ti_i2c1;
SPI_Regs fake_ti_spi0;
SPI_Regs fake_ti_spi1;
ADC12_Regs fake_ti_adc0;
ADC12_Regs fake_ti_adc1;
GPTIMER_Regs fake_ti_timg0;
GPTIMER_Regs fake_ti_timg1;
GPTIMER_Regs fake_ti_tima0;
GPTIMER_Regs fake_ti_tima1;

bool fake_ti_uart_tx_fifo_full = false;
bool fake_ti_uart_rx_fifo_empty = true;
uint8_t fake_ti_uart_next_rx_byte = 0;
DL_UART_MAIN_IIDX fake_ti_uart_pending_interrupt = DL_UART_MAIN_IIDX_NONE;
uint8_t fake_ti_uart_last_tx_byte = 0;

uint32_t fake_ti_i2c_status = DL_I2C_CONTROLLER_STATUS_IDLE;
uint8_t fake_ti_i2c_next_rx_byte = 0;
DL_I2C_IIDX fake_ti_i2c_pending_interrupt = DL_I2C_IIDX_NONE;

uint8_t fake_ti_spi_next_rx_byte = 0;
DL_SPI_IIDX fake_ti_spi_pending_interrupt = DL_SPI_IIDX_NONE;

uint16_t fake_ti_adc_next_value = 0;
DL_ADC12_IIDX fake_ti_adc_pending_interrupt = DL_ADC12_IIDX_MEM0_RESULT_LOADED;

DL_TIMER_IIDX fake_ti_timer_pending_interrupt = DL_TIMER_IIDX_NONE;

static uint32_t s_cc_value[2][4]; /* [TIMA0/TIMA1][CC0..CC3] */

static uint32_t s_gpio_state[2];

static int hf_test_gpio_index(GPIO_Regs *port)
{
    if (port == &fake_ti_gpioa) { return 0; }
    if (port == &fake_ti_gpiob) { return 1; }
    return -1;
}

void fake_ti_gpio_set_input(GPIO_Regs *port, uint32_t mask, bool state)
{
    int index = hf_test_gpio_index(port);
    if (index < 0) { return; }
    if (state) { s_gpio_state[index] |= mask; } else { s_gpio_state[index] &= ~mask; }
}

void DL_GPIO_enableOutput(GPIO_Regs *port, uint32_t mask) { (void)port; (void)mask; }
void DL_GPIO_disableOutput(GPIO_Regs *port, uint32_t mask) { (void)port; (void)mask; }

void DL_GPIO_setPins(GPIO_Regs *port, uint32_t mask)
{
    int index = hf_test_gpio_index(port);
    if (index >= 0) { s_gpio_state[index] |= mask; }
}

void DL_GPIO_clearPins(GPIO_Regs *port, uint32_t mask)
{
    int index = hf_test_gpio_index(port);
    if (index >= 0) { s_gpio_state[index] &= ~mask; }
}

void DL_GPIO_togglePins(GPIO_Regs *port, uint32_t mask)
{
    int index = hf_test_gpio_index(port);
    if (index >= 0) { s_gpio_state[index] ^= mask; }
}

uint32_t DL_GPIO_readPins(GPIO_Regs *port, uint32_t mask)
{
    int index = hf_test_gpio_index(port);
    return (index >= 0) ? (s_gpio_state[index] & mask) : 0U;
}

void DL_UART_Main_enableInterrupt(UART_Regs *uart, uint32_t flags) { (void)uart; (void)flags; }
bool DL_UART_Main_isTXFIFOFull(UART_Regs *uart) { (void)uart; return fake_ti_uart_tx_fifo_full; }

void DL_UART_Main_transmitData(UART_Regs *uart, uint8_t data)
{
    (void)uart;
    fake_ti_uart_last_tx_byte = data;
}

DL_UART_MAIN_IIDX DL_UART_Main_getPendingInterrupt(UART_Regs *uart)
{
    (void)uart;
    return fake_ti_uart_pending_interrupt;
}

bool DL_UART_Main_isRXFIFOEmpty(UART_Regs *uart) { (void)uart; return fake_ti_uart_rx_fifo_empty; }

uint8_t DL_UART_Main_receiveData(UART_Regs *uart)
{
    (void)uart;
    /* The adapter drains with `while (!isRXFIFOEmpty()) { ... receiveData(); }`
     * - auto-report empty right after one byte is read so a
     * single-byte test scenario terminates that loop without the fake
     * needing to interleave with the adapter's own C code. */
    fake_ti_uart_rx_fifo_empty = true;
    return fake_ti_uart_next_rx_byte;
}

uint32_t DL_I2C_getControllerStatus(I2C_Regs *i2c) { (void)i2c; return fake_ti_i2c_status; }
void DL_I2C_fillControllerTXFIFO(I2C_Regs *i2c, uint8_t *data, uint32_t len) { (void)i2c; (void)data; (void)len; }

void DL_I2C_startControllerTransfer(I2C_Regs *i2c, uint8_t addr, DL_I2C_CONTROLLER_DIRECTION dir, uint32_t len)
{
    (void)i2c; (void)addr; (void)dir; (void)len;
}

void DL_I2C_startControllerTransferAdvanced(I2C_Regs *i2c, uint8_t addr, DL_I2C_CONTROLLER_DIRECTION dir,
                                            uint32_t len, DL_I2C_CONTROLLER_START start,
                                            DL_I2C_CONTROLLER_STOP stop, DL_I2C_CONTROLLER_ACK ack)
{
    (void)i2c; (void)addr; (void)dir; (void)len; (void)start; (void)stop; (void)ack;
}

uint8_t DL_I2C_receiveControllerData(I2C_Regs *i2c) { (void)i2c; return fake_ti_i2c_next_rx_byte; }
void DL_I2C_enableInterrupt(I2C_Regs *i2c, uint32_t flags) { (void)i2c; (void)flags; }
DL_I2C_IIDX DL_I2C_getPendingInterrupt(I2C_Regs *i2c) { (void)i2c; return fake_ti_i2c_pending_interrupt; }

void DL_SPI_init(SPI_Regs *spi, DL_SPI_Config *config) { (void)spi; (void)config; }
void DL_SPI_enable(SPI_Regs *spi) { (void)spi; }
void DL_SPI_transmitDataBlocking8(SPI_Regs *spi, uint8_t data) { (void)spi; (void)data; }
uint8_t DL_SPI_receiveDataBlocking8(SPI_Regs *spi) { (void)spi; return fake_ti_spi_next_rx_byte; }
void DL_SPI_enableInterrupt(SPI_Regs *spi, uint32_t flags) { (void)spi; (void)flags; }
void DL_SPI_disableInterrupt(SPI_Regs *spi, uint32_t flags) { (void)spi; (void)flags; }
DL_SPI_IIDX DL_SPI_getPendingInterrupt(SPI_Regs *spi) { (void)spi; return fake_ti_spi_pending_interrupt; }

void DL_ADC12_reset(ADC12_Regs *adc) { (void)adc; }
void DL_ADC12_enablePower(ADC12_Regs *adc) { (void)adc; }
void DL_ADC12_setSampleTime0(ADC12_Regs *adc, uint32_t cycles) { (void)adc; (void)cycles; }

void DL_ADC12_initSingleSample(ADC12_Regs *adc, DL_ADC12_REPEAT_MODE repeat, DL_ADC12_SAMPLING_SOURCE source,
                                DL_ADC12_TRIG_SRC trig, DL_ADC12_SAMP_CONV_RES res,
                                DL_ADC12_SAMP_CONV_DATA_FORMAT format)
{
    (void)adc; (void)repeat; (void)source; (void)trig; (void)res; (void)format;
}

void DL_ADC12_enableConversions(ADC12_Regs *adc) { (void)adc; }

void DL_ADC12_configConversionMem(ADC12_Regs *adc, DL_ADC12_MEM_IDX idx, DL_ADC12_INPUT_CHAN chansel,
                                  DL_ADC12_REFERENCE_VOLTAGE vref, DL_ADC12_SAMPLE_TIMER_SOURCE stime,
                                  DL_ADC12_AVERAGING_MODE avgen, DL_ADC12_BURN_OUT_SOURCE bcsen,
                                  DL_ADC12_TRIGGER_MODE trig, DL_ADC12_WINDOWS_COMP_MODE wincomp)
{
    (void)adc; (void)idx; (void)chansel; (void)vref; (void)stime;
    (void)avgen; (void)bcsen; (void)trig; (void)wincomp;
}

void DL_ADC12_startConversion(ADC12_Regs *adc) { (void)adc; }
DL_ADC12_IIDX DL_ADC12_getPendingInterrupt(ADC12_Regs *adc) { (void)adc; return fake_ti_adc_pending_interrupt; }
uint16_t DL_ADC12_getMemResult(ADC12_Regs *adc, DL_ADC12_MEM_IDX idx) { (void)adc; (void)idx; return fake_ti_adc_next_value; }
void DL_ADC12_enableInterrupt(ADC12_Regs *adc, uint32_t flags) { (void)adc; (void)flags; }
void DL_ADC12_disableInterrupt(ADC12_Regs *adc, uint32_t flags) { (void)adc; (void)flags; }

void DL_Timer_setClockConfig(GPTIMER_Regs *timer, DL_Timer_ClockConfig const *config) { (void)timer; (void)config; }
void DL_Timer_initTimerMode(GPTIMER_Regs *timer, DL_Timer_TimerConfig const *config) { (void)timer; (void)config; }
void DL_Timer_enableInterrupt(GPTIMER_Regs *timer, uint32_t flags) { (void)timer; (void)flags; }
void DL_Timer_startCounter(GPTIMER_Regs *timer) { (void)timer; }
void DL_Timer_stopCounter(GPTIMER_Regs *timer) { (void)timer; }
DL_TIMER_IIDX DL_Timer_getPendingInterrupt(GPTIMER_Regs *timer) { (void)timer; return fake_ti_timer_pending_interrupt; }

void DL_Timer_initFourCCPWMMode(GPTIMER_Regs *timer, DL_Timer_PWMConfig const *config) { (void)timer; (void)config; }

static int hf_test_cc_timer_index(GPTIMER_Regs *timer)
{
    if (timer == &fake_ti_tima0) { return 0; }
    if (timer == &fake_ti_tima1) { return 1; }
    return -1;
}

void DL_Timer_setCaptureCompareValue(GPTIMER_Regs *timer, uint32_t value, DL_TIMER_CC_INDEX ccIndex)
{
    int t = hf_test_cc_timer_index(timer);
    if (t >= 0 && ccIndex >= 0 && ccIndex < 4) {
        s_cc_value[t][ccIndex] = value;
    }
}

uint32_t DL_Timer_getCaptureCompareValue(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX ccIndex)
{
    int t = hf_test_cc_timer_index(timer);
    return (t >= 0 && ccIndex >= 0 && ccIndex < 4) ? s_cc_value[t][ccIndex] : 0;
}

uint32_t fake_ti_get_cc_value(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX ccIndex)
{
    return DL_Timer_getCaptureCompareValue(timer, ccIndex);
}

void fake_ti_reset(void)
{
    memset(s_gpio_state, 0, sizeof(s_gpio_state));
    fake_ti_uart_tx_fifo_full = false;
    fake_ti_uart_rx_fifo_empty = true;
    fake_ti_uart_next_rx_byte = 0;
    fake_ti_uart_pending_interrupt = DL_UART_MAIN_IIDX_NONE;
    fake_ti_uart_last_tx_byte = 0;
    fake_ti_i2c_status = DL_I2C_CONTROLLER_STATUS_IDLE;
    fake_ti_i2c_next_rx_byte = 0;
    fake_ti_i2c_pending_interrupt = DL_I2C_IIDX_NONE;
    fake_ti_spi_next_rx_byte = 0;
    fake_ti_spi_pending_interrupt = DL_SPI_IIDX_NONE;
    fake_ti_adc_next_value = 0;
    fake_ti_adc_pending_interrupt = DL_ADC12_IIDX_MEM0_RESULT_LOADED;
    fake_ti_timer_pending_interrupt = DL_TIMER_IIDX_NONE;
    memset(s_cc_value, 0, sizeof(s_cc_value));
}
