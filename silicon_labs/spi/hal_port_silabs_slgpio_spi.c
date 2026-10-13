/* Silicon Labs adapter for the SPI portion of hal_wrapper.h, against
 * SPIDRV (platform/emdrv/spidrv) — gated by TARGET_SILABS_SLGPIO, the
 * same target macro the current sl_gpio/sl_i2c adapters use.
 *
 * Genuinely different situation from GPIO/I2C's classic-vs-current
 * split: SPIDRV is used UNCHANGED for both this target and
 * TARGET_SILABS (see hal_port_silabs_spi.c, nearly identical to this
 * file) — verified directly against
 * platform/emdrv/spidrv/inc/spidrv.h in SiliconLabs/simplicity_sdk
 * (sisdk-2025.6): its handle struct supports both classic USART_TypeDef
 * and the newer EUSART_TypeDef (Series 3) internally, so there is no
 * separate "current" SPI driver the way sl_gpio/sl_i2c are — same
 * "document the finding, don't force an artificial split" treatment
 * as the UART adapters' sl_iostream_usart finding, except here the
 * shared driver genuinely offers what we need (real blocking AND
 * async variants with a proper completion callback), unlike
 * sl_iostream's limited surface for UART. Gated by
 * TARGET_SILABS_SLGPIO purely for consistency with this vendor's
 * other current-driver adapters, not because the implementation
 * differs.
 *
 * `csControl = spidrvCsControlApplication` tells SPIDRV NOT to drive
 * chip-select itself — this wrapper manages it via the `cs_pin`
 * argument on every call instead, same as every other vendor's SPI
 * adapter.
 *
 * IMPORTANT hardware-sharing note, same one that applies to this
 * vendor's UART adapter (hal_port_silabs_slgpio_uart.c): classic
 * EFM32/EFR32 SPI and UART both run on the same USART peripheral
 * blocks. SPIDRV manages its own interrupts internally for whichever
 * USART instance you give it. If you enable both this file and
 * hal_port_silabs_slgpio_uart.c for the SAME physical USART instance
 * in the same project, you WILL get a duplicate interrupt-vector link
 * error — trim each file's instance table so UART and SPI use
 * disjoint USART instances, the same "loud, not silent" trim-the-table
 * precedent already established for STM32's I2C handle table.
 *
 * Checked (not a gap): unlike this vendor's other classic-emlib-shaped
 * adapters, which turned out to need their own explicit
 * CMU_ClockEnable(), SPIDRV_Init() already enables its own GPIO and
 * USART/EUSART bus clocks internally — confirmed against
 * platform/emdrv/spidrv/src/spidrv.c in SiliconLabs/simplicity_sdk. No
 * fix needed here.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_SPI

#include "spidrv.h"

#ifdef USART0
#define HF_SILABS_SPI_PORT_0 USART0
#else
#define HF_SILABS_SPI_PORT_0 NULL
#endif
#ifdef USART1
#define HF_SILABS_SPI_PORT_1 USART1
#else
#define HF_SILABS_SPI_PORT_1 NULL
#endif

static void * const s_spi_port_table[] = { HF_SILABS_SPI_PORT_0, HF_SILABS_SPI_PORT_1 };
#define HF_SILABS_SPI_COUNT (sizeof(s_spi_port_table) / sizeof(s_spi_port_table[0]))

static SPIDRV_HandleData_t s_handle_data[HF_SILABS_SPI_COUNT];
static hf_gpio_t s_active_cs[HF_SILABS_SPI_COUNT];
static bool s_is_read[HF_SILABS_SPI_COUNT];
static volatile bool s_busy[HF_SILABS_SPI_COUNT];
static bool s_is_async[HF_SILABS_SPI_COUNT];
static hf_spi_tx_callback_t s_tx_callback[HF_SILABS_SPI_COUNT];
static hf_spi_rx_callback_t s_rx_callback[HF_SILABS_SPI_COUNT];

static SPIDRV_Handle_t hf_decode_spi(hf_spi_t instance)
{
    if (instance >= HF_SILABS_SPI_COUNT || s_spi_port_table[instance] == NULL) {
        return NULL;
    }
    return &s_handle_data[instance];
}

static int hf_spi_index_of(SPIDRV_Handle_t handle)
{
    size_t i;
    for (i = 0; i < HF_SILABS_SPI_COUNT; i++) {
        if (&s_handle_data[i] == handle) {
            return (int)i;
        }
    }
    return -1;
}

static void hf_spi_completion(SPIDRV_Handle_t handle, Ecode_t transferStatus, int itemsTransferred)
{
    (void)itemsTransferred;
    int index = hf_spi_index_of(handle);
    if (index < 0) {
        return;
    }
    hf_gpio_write(s_active_cs[index], true);
    s_busy[index] = false;
    if (!s_is_async[index]) {
        return;
    }
    s_is_async[index] = false;
    if (transferStatus != ECODE_OK) {
        /* hf_spi_tx/rx_callback_t carry no success/failure signal
         * (same reasoning as UART's callbacks) — stay silent on
         * error rather than invoke the callback and imply success. */
        return;
    }
    if (s_is_read[index]) {
        if (s_rx_callback[index] != NULL) {
            s_rx_callback[index]((hf_spi_t)index);
        }
    } else {
        if (s_tx_callback[index] != NULL) {
            s_tx_callback[index]((hf_spi_t)index);
        }
    }
}

bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode)
{
    if (instance >= HF_SILABS_SPI_COUNT || s_spi_port_table[instance] == NULL) {
        return false;
    }

    SPIDRV_ClockMode_t clock_mode;
    switch (mode) {
    case HF_SPI_MODE_0: clock_mode = spidrvClockMode0; break;
    case HF_SPI_MODE_1: clock_mode = spidrvClockMode1; break;
    case HF_SPI_MODE_2: clock_mode = spidrvClockMode2; break;
    case HF_SPI_MODE_3: clock_mode = spidrvClockMode3; break;
    default: return false;
    }

    SPIDRV_Init_t init = {0};
    init.port = s_spi_port_table[instance];
    init.bitRate = clock_hz;
    init.frameLength = 8;
    init.dummyTxValue = 0xFF;
    init.type = spidrvMaster;
    init.bitOrder = spidrvBitOrderMsbFirst;
    init.clockMode = clock_mode;
    init.csControl = spidrvCsControlApplication;

    return SPIDRV_Init(&s_handle_data[instance], &init) == ECODE_OK;
}

bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPIDRV_Handle_t handle = hf_decode_spi(instance);
    if (handle == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    Ecode_t status = SPIDRV_MTransmitB(handle, data, (int)len);
    hf_gpio_write(cs_pin, true);
    return status == ECODE_OK;
}

bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPIDRV_Handle_t handle = hf_decode_spi(instance);
    if (handle == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    Ecode_t status = SPIDRV_MReceiveB(handle, data, (int)len);
    hf_gpio_write(cs_pin, true);
    return status == ECODE_OK;
}

bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len)
{
    SPIDRV_Handle_t handle = hf_decode_spi(instance);
    if (handle == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    Ecode_t status = SPIDRV_MTransferB(handle, tx_data, rx_data, (int)len);
    hf_gpio_write(cs_pin, true);
    return status == ECODE_OK;
}

void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback)
{
    if (instance < HF_SILABS_SPI_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback)
{
    if (instance < HF_SILABS_SPI_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPIDRV_Handle_t handle = hf_decode_spi(instance);
    if (handle == NULL || s_busy[instance]) {
        return false;
    }
    s_busy[instance] = true;
    s_is_async[instance] = true;
    s_is_read[instance] = false;
    s_active_cs[instance] = cs_pin;
    hf_gpio_write(cs_pin, false);
    if (SPIDRV_MTransmit(handle, data, (int)len, hf_spi_completion) != ECODE_OK) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        s_is_async[instance] = false;
        return false;
    }
    return true;
}

bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPIDRV_Handle_t handle = hf_decode_spi(instance);
    if (handle == NULL || s_busy[instance]) {
        return false;
    }
    s_busy[instance] = true;
    s_is_async[instance] = true;
    s_is_read[instance] = true;
    s_active_cs[instance] = cs_pin;
    hf_gpio_write(cs_pin, false);
    if (SPIDRV_MReceive(handle, data, (int)len, hf_spi_completion) != ECODE_OK) {
        hf_gpio_write(cs_pin, true);
        s_busy[instance] = false;
        s_is_async[instance] = false;
        return false;
    }
    return true;
}

#endif /* HF_ENABLE_SPI */
#endif /* TARGET_SILABS_SLGPIO */
