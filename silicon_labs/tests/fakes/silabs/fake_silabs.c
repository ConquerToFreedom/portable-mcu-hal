/* Fake classic emlib implementation - backs em_gpio.h/em_usart.h/
 * em_i2c.h/em_adc.h/spidrv.h's declarations with host-runnable bodies
 * so this vendor's classic (TARGET_SILABS) adapters, and the SPI/UART
 * adapters shared with TARGET_SILABS_SLGPIO, can be unit tested
 * without real Silicon Labs silicon. Not a copy of Silicon Labs' emlib
 * source.
 */
#include "fake_silabs.h"
#include <string.h>

USART_TypeDef fake_silabs_usart0;
USART_TypeDef fake_silabs_usart1;
I2C_TypeDef fake_silabs_i2c0;
I2C_TypeDef fake_silabs_i2c1;
ADC_TypeDef fake_silabs_adc0;
ADC_TypeDef fake_silabs_adc1;
TIMER_TypeDef fake_silabs_timer0_storage;
TIMER_TypeDef fake_silabs_timer1_storage;
TIMER_TypeDef fake_silabs_timer2_storage;
TIMER_TypeDef fake_silabs_timer3_storage;
GPIO_TypeDef_Peripheral fake_silabs_gpio_peripheral;

I2C_TransferReturn_TypeDef fake_silabs_i2c_next_result = i2cTransferDone;
Ecode_t fake_silabs_spi_next_status = ECODE_OK;
bool fake_silabs_spi_auto_complete = true;
uint32_t fake_silabs_adc_next_value = 0;

static bool s_gpio_state[6][16];
static uint32_t s_usart_flags[2];
static uint32_t s_timer_flags[4];
static uint32_t s_cc_compare[4][4]; /* [TIMER0..3][CC0..3] */

static int hf_test_timer_index(TIMER_TypeDef *timer)
{
    if (timer == TIMER0) { return 0; }
    if (timer == TIMER1) { return 1; }
    if (timer == TIMER2) { return 2; }
    if (timer == TIMER3) { return 3; }
    return -1;
}

void fake_silabs_gpio_set_input(GPIO_Port_TypeDef port, unsigned int pin, bool state)
{
    if ((int)port >= 0 && port < 6 && pin < 16) {
        s_gpio_state[port][pin] = state;
    }
}

void GPIO_PinModeSet(GPIO_Port_TypeDef port, unsigned int pin, GPIO_Mode_TypeDef mode, unsigned int out)
{
    (void)port; (void)pin; (void)mode; (void)out;
}

void CMU_ClockEnable(CMU_Clock_TypeDef clock, bool enable)
{
    (void)clock; (void)enable;
}

void GPIO_PinOutSet(GPIO_Port_TypeDef port, unsigned int pin)
{
    if (port < 6 && pin < 16) { s_gpio_state[port][pin] = true; }
}

void GPIO_PinOutClear(GPIO_Port_TypeDef port, unsigned int pin)
{
    if (port < 6 && pin < 16) { s_gpio_state[port][pin] = false; }
}

unsigned int GPIO_PinInGet(GPIO_Port_TypeDef port, unsigned int pin)
{
    return (port < 6 && pin < 16 && s_gpio_state[port][pin]) ? 1U : 0U;
}

void GPIO_PinOutToggle(GPIO_Port_TypeDef port, unsigned int pin)
{
    if (port < 6 && pin < 16) { s_gpio_state[port][pin] = !s_gpio_state[port][pin]; }
}

void NVIC_EnableIRQ(int irqn) { (void)irqn; }

static int hf_test_usart_index(USART_TypeDef *usart)
{
    if (usart == &fake_silabs_usart0) { return 0; }
    if (usart == &fake_silabs_usart1) { return 1; }
    return -1;
}

void USART_InitAsync(USART_TypeDef *usart, USART_InitAsync_TypeDef const *init)
{
    (void)usart; (void)init;
}

void USART_IntClear(USART_TypeDef *usart, uint32_t flags)
{
    int index = hf_test_usart_index(usart);
    if (index >= 0) {
        s_usart_flags[index] &= ~flags;
    }
}

void USART_IntEnable(USART_TypeDef *usart, uint32_t flags) { (void)usart; (void)flags; }
void USART_IntDisable(USART_TypeDef *usart, uint32_t flags) { (void)usart; (void)flags; }

uint32_t USART_IntGet(USART_TypeDef *usart)
{
    int index = hf_test_usart_index(usart);
    return (index >= 0) ? s_usart_flags[index] : 0U;
}

void USART_Tx(USART_TypeDef *usart, uint8_t data)
{
    usart->TXDATA = data;
}

void fake_silabs_usart_set_flags(int instance, uint32_t flags)
{
    if (instance >= 0 && instance < 2) {
        s_usart_flags[instance] = flags;
    }
}

void I2C_Init(I2C_TypeDef *i2c, I2C_Init_TypeDef const *init) { (void)i2c; (void)init; }

I2C_TransferReturn_TypeDef I2C_TransferInit(I2C_TypeDef *i2c, I2C_TransferSeq_TypeDef *seq)
{
    (void)i2c; (void)seq;
    return i2cTransferInProgress;
}

I2C_TransferReturn_TypeDef I2C_Transfer(I2C_TypeDef *i2c)
{
    (void)i2c;
    return fake_silabs_i2c_next_result;
}

Ecode_t SPIDRV_Init(SPIDRV_Handle_t handle, SPIDRV_Init_t *initData)
{
    (void)handle; (void)initData;
    return fake_silabs_spi_next_status;
}

Ecode_t SPIDRV_MTransmitB(SPIDRV_Handle_t handle, void const *buffer, int count)
{
    (void)handle; (void)buffer; (void)count;
    return fake_silabs_spi_next_status;
}

Ecode_t SPIDRV_MReceiveB(SPIDRV_Handle_t handle, void *buffer, int count)
{
    (void)handle; (void)buffer; (void)count;
    return fake_silabs_spi_next_status;
}

Ecode_t SPIDRV_MTransferB(SPIDRV_Handle_t handle, void const *txBuffer, void *rxBuffer, int count)
{
    (void)handle; (void)txBuffer; (void)rxBuffer; (void)count;
    return fake_silabs_spi_next_status;
}

static SPIDRV_Handle_t s_spi_last_handle;
static SPIDRV_Callback_t s_spi_last_callback;
static int s_spi_last_count;

Ecode_t SPIDRV_MTransmit(SPIDRV_Handle_t handle, void const *buffer, int count, SPIDRV_Callback_t callback)
{
    (void)buffer;
    s_spi_last_handle = handle;
    s_spi_last_callback = callback;
    s_spi_last_count = count;
    if (fake_silabs_spi_auto_complete && callback != NULL) {
        callback(handle, fake_silabs_spi_next_status, count);
    }
    return fake_silabs_spi_next_status;
}

Ecode_t SPIDRV_MReceive(SPIDRV_Handle_t handle, void *buffer, int count, SPIDRV_Callback_t callback)
{
    (void)buffer;
    s_spi_last_handle = handle;
    s_spi_last_callback = callback;
    s_spi_last_count = count;
    if (fake_silabs_spi_auto_complete && callback != NULL) {
        callback(handle, fake_silabs_spi_next_status, count);
    }
    return fake_silabs_spi_next_status;
}

void fake_silabs_spi_fire_completion(SPIDRV_Handle_t handle, Ecode_t status, int items_transferred)
{
    (void)handle; (void)items_transferred;
    if (s_spi_last_callback != NULL) {
        s_spi_last_callback(s_spi_last_handle, status, s_spi_last_count);
    }
}

void ADC_Init(ADC_TypeDef *adc, ADC_Init_TypeDef const *init) { (void)adc; (void)init; }
void ADC_InitSingle(ADC_TypeDef *adc, ADC_InitSingle_TypeDef const *init) { (void)adc; (void)init; }
void ADC_Start(ADC_TypeDef *adc, ADC_Start_TypeDef cmd) { (void)adc; (void)cmd; }

uint32_t ADC_IntGet(ADC_TypeDef *adc) { (void)adc; return ADC_IF_SINGLE; }
void ADC_IntClear(ADC_TypeDef *adc, uint32_t flags) { (void)adc; (void)flags; }
void ADC_IntEnable(ADC_TypeDef *adc, uint32_t flags) { (void)adc; (void)flags; }
void ADC_IntDisable(ADC_TypeDef *adc, uint32_t flags) { (void)adc; (void)flags; }

uint32_t ADC_DataSingleGet(ADC_TypeDef *adc)
{
    (void)adc;
    return fake_silabs_adc_next_value;
}

void TIMER_Init(TIMER_TypeDef *timer, TIMER_Init_TypeDef const *init) { (void)timer; (void)init; }
void TIMER_TopSet(TIMER_TypeDef *timer, uint32_t val) { (void)timer; (void)val; }
void TIMER_CounterSet(TIMER_TypeDef *timer, uint32_t val) { (void)timer; (void)val; }
void TIMER_Enable(TIMER_TypeDef *timer, bool enable) { (void)timer; (void)enable; }

void TIMER_IntClear(TIMER_TypeDef *timer, uint32_t flags)
{
    int i = hf_test_timer_index(timer);
    if (i >= 0) { s_timer_flags[i] &= ~flags; }
}

void TIMER_IntEnable(TIMER_TypeDef *timer, uint32_t flags) { (void)timer; (void)flags; }

uint32_t TIMER_IntGet(TIMER_TypeDef *timer)
{
    int i = hf_test_timer_index(timer);
    return (i >= 0) ? s_timer_flags[i] : 0U;
}

void fake_silabs_timer_set_flags(int instance, uint32_t flags)
{
    if (instance >= 0 && instance < 4) {
        s_timer_flags[instance] = flags;
    }
}

void TIMER_InitCC(TIMER_TypeDef *timer, unsigned int ch, TIMER_InitCC_TypeDef const *init)
{
    (void)timer; (void)ch; (void)init;
}

void TIMER_CompareSet(TIMER_TypeDef *timer, unsigned int ch, uint32_t val)
{
    int i = hf_test_timer_index(timer);
    if (i >= 0 && ch < 4) { s_cc_compare[i][ch] = val; }
}

void TIMER_CompareBufSet(TIMER_TypeDef *timer, unsigned int ch, uint32_t val)
{
    int i = hf_test_timer_index(timer);
    if (i >= 0 && ch < 4) { s_cc_compare[i][ch] = val; }
}

uint32_t fake_silabs_pwm_get_compare(TIMER_TypeDef *timer, unsigned int ch)
{
    int i = hf_test_timer_index(timer);
    return (i >= 0 && ch < 4) ? s_cc_compare[i][ch] : 0U;
}

int fake_silabs_timer_num(TIMER_TypeDef *timer)
{
    return hf_test_timer_index(timer);
}

uint32_t fake_silabs_pwm_get_route(TIMER_TypeDef *timer, unsigned int ch)
{
    int i = hf_test_timer_index(timer);
    if (i < 0 || ch >= 4) { return 0U; }
    uint32_t const *route = &fake_silabs_gpio_peripheral.TIMERROUTE[i].CC0ROUTE;
    return route[ch];
}

bool fake_silabs_pwm_route_enabled(TIMER_TypeDef *timer, unsigned int ch)
{
    int i = hf_test_timer_index(timer);
    if (i < 0) { return false; }
    return (fake_silabs_gpio_peripheral.TIMERROUTE_SET[i].ROUTEEN & (1U << ch)) != 0U;
}

void fake_silabs_reset(void)
{
    memset(s_gpio_state, 0, sizeof(s_gpio_state));
    memset(s_usart_flags, 0, sizeof(s_usart_flags));
    memset(&fake_silabs_usart0, 0, sizeof(fake_silabs_usart0));
    memset(&fake_silabs_usart1, 0, sizeof(fake_silabs_usart1));
    fake_silabs_i2c_next_result = i2cTransferDone;
    fake_silabs_spi_next_status = ECODE_OK;
    fake_silabs_spi_auto_complete = true;
    fake_silabs_adc_next_value = 0;
    s_spi_last_handle = NULL;
    s_spi_last_callback = NULL;
    s_spi_last_count = 0;
    memset(s_timer_flags, 0, sizeof(s_timer_flags));
    memset(s_cc_compare, 0, sizeof(s_cc_compare));
    memset(&fake_silabs_gpio_peripheral, 0, sizeof(fake_silabs_gpio_peripheral));
}
