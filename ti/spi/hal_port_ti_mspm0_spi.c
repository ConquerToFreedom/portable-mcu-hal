/* TI MSPM0 (driverlib) vendor adapter for the SPI portion of
 * hal_wrapper.h.
 *
 * Assumes the customer's SysConfig-generated "ti_msp_dl_config.h" is
 * on the include path, same assumption as this vendor's other
 * adapters. `clock_hz` is advisory only, same category of limitation
 * as this vendor's I2C/UART clock/baud parameters: DL_SPI_ClockConfig
 * is a clock-source-plus-divider pair, and computing the right
 * divider for an arbitrary Hz target needs knowing which clock feeds
 * this specific SPI instance — assumed already configured correctly
 * by SysConfig's generated init instead of recomputed here.
 *
 * Verified this file's core API — DL_SPI_Config/DL_SPI_ClockConfig,
 * DL_SPI_init, DL_SPI_transmitDataBlocking8/receiveDataBlocking8, the
 * DL_SPI_FRAME_FORMAT_MOTO3_POL*_PHA* mode constants, and
 * DL_SPI_CHIP_SELECT_NONE — directly against
 * TexasInstruments/mspm0-sdk's own source/ti/driverlib/dl_spi.h on
 * GitHub, unlike this vendor's I2C/UART adapters, which had to fall
 * back to web search snippets and community examples because TI's
 * docs domain and forum are both blocked in this environment. Only
 * one real inference remains, not independently confirmed by a worked
 * example: DL_SPI_receiveDataBlocking8() takes no dummy-TX-byte
 * parameter, so — consistent with how SPI hardware always generates
 * its clock from an active transmit — a receive-only call here first
 * transmits a dummy byte to produce the clock edges, then reads the
 * FIFO. The two remaining names in the async tier
 * (DL_SPI_getPendingInterrupt, DL_SPI_disableInterrupt) follow
 * driverlib's confirmed, consistent naming pattern from other
 * peripherals (DL_I2C_getPendingInterrupt, DL_GPIO_disableInterrupt)
 * but weren't in this exact fetch — flagged the same way as this
 * vendor's I2C/UART adapters flag their own unconfirmed specifics.
 *
 * DL_SPI_CHIP_SELECT_NONE disables the peripheral's own automatic CS
 * output — this wrapper manages chip-select itself via the `cs_pin`
 * argument on every call, same as every other vendor's SPI adapter.
 *
 * hf_spi_write()/read() drain or dummy-feed the FIFO one byte at a
 * time (no whole-buffer transfer primitive exists at this level, same
 * shape as this vendor's classic-Silicon-Labs-style byte loop) — every
 * transmitted byte has a corresponding received byte that must be
 * drained regardless of whether the caller wants it, or the RX FIFO
 * would overflow.
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_SPI

#include "ti_msp_dl_config.h"

#ifdef SPI0
#define HF_MSPM0_SPI_0 SPI0
#else
#define HF_MSPM0_SPI_0 NULL
#endif
#ifdef SPI1
#define HF_MSPM0_SPI_1 SPI1
#else
#define HF_MSPM0_SPI_1 NULL
#endif

static SPI_Regs * const s_spi_table[] = { HF_MSPM0_SPI_0, HF_MSPM0_SPI_1 };
#define HF_MSPM0_SPI_COUNT (sizeof(s_spi_table) / sizeof(s_spi_table[0]))

static const uint8_t *s_tx_data[HF_MSPM0_SPI_COUNT];
static uint8_t *s_rx_data[HF_MSPM0_SPI_COUNT];
static uint32_t s_len[HF_MSPM0_SPI_COUNT];
static uint32_t s_pos[HF_MSPM0_SPI_COUNT];
static volatile bool s_busy[HF_MSPM0_SPI_COUNT];
static bool s_is_read[HF_MSPM0_SPI_COUNT];
static hf_gpio_t s_active_cs[HF_MSPM0_SPI_COUNT];
static hf_spi_tx_callback_t s_tx_callback[HF_MSPM0_SPI_COUNT];
static hf_spi_rx_callback_t s_rx_callback[HF_MSPM0_SPI_COUNT];

static SPI_Regs *hf_decode_spi(hf_spi_t instance)
{
    if (instance >= HF_MSPM0_SPI_COUNT) {
        return NULL;
    }
    return s_spi_table[instance];
}

bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL) {
        return false;
    }
    (void)clock_hz; /* advisory only on this vendor — see file header comment */

    DL_SPI_FRAME_FORMAT frame_format;
    switch (mode) {
    case HF_SPI_MODE_0: frame_format = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0; break;
    case HF_SPI_MODE_1: frame_format = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA1; break;
    case HF_SPI_MODE_2: frame_format = DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA0; break;
    case HF_SPI_MODE_3: frame_format = DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA1; break;
    default: return false;
    }

    DL_SPI_Config config = {0};
    config.mode = DL_SPI_MODE_CONTROLLER;
    config.frameFormat = frame_format;
    config.parity = DL_SPI_PARITY_NONE;
    config.dataSize = DL_SPI_DATA_SIZE_8;
    config.bitOrder = DL_SPI_BIT_ORDER_MSB_FIRST;
    config.chipSelectPin = DL_SPI_CHIP_SELECT_NONE;

    DL_SPI_init(spi, &config);
    DL_SPI_enable(spi);
    return true;
}

bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_SPI_transmitDataBlocking8(spi, data[i]);
        (void)DL_SPI_receiveDataBlocking8(spi); /* drain, or the RX FIFO overflows */
    }
    hf_gpio_write(cs_pin, true);
    return true;
}

bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_SPI_transmitDataBlocking8(spi, 0xFF); /* dummy byte — generates the clock */
        data[i] = DL_SPI_receiveDataBlocking8(spi);
    }
    hf_gpio_write(cs_pin, true);
    return true;
}

bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL) {
        return false;
    }
    hf_gpio_write(cs_pin, false);
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_SPI_transmitDataBlocking8(spi, tx_data[i]);
        rx_data[i] = DL_SPI_receiveDataBlocking8(spi);
    }
    hf_gpio_write(cs_pin, true);
    return true;
}

void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback)
{
    if (instance < HF_MSPM0_SPI_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback)
{
    if (instance < HF_MSPM0_SPI_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL || s_busy[instance] || len == 0) {
        return false;
    }
    s_tx_data[instance] = data;
    s_rx_data[instance] = NULL;
    s_len[instance] = len;
    s_pos[instance] = 1;
    s_is_read[instance] = false;
    s_active_cs[instance] = cs_pin;
    s_busy[instance] = true;
    hf_gpio_write(cs_pin, false);
    DL_SPI_enableInterrupt(spi, DL_SPI_INTERRUPT_RX);
    DL_SPI_transmitDataBlocking8(spi, data[0]); /* kicks off the sequence; the RX interrupt drives the rest */
    return true;
}

bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len)
{
    SPI_Regs *spi = hf_decode_spi(instance);
    if (spi == NULL || s_busy[instance] || len == 0) {
        return false;
    }
    s_tx_data[instance] = NULL;
    s_rx_data[instance] = data;
    s_len[instance] = len;
    s_pos[instance] = 1;
    s_is_read[instance] = true;
    s_active_cs[instance] = cs_pin;
    s_busy[instance] = true;
    hf_gpio_write(cs_pin, false);
    DL_SPI_enableInterrupt(spi, DL_SPI_INTERRUPT_RX);
    DL_SPI_transmitDataBlocking8(spi, 0xFF);
    return true;
}

static void hf_spi_service_irq(hf_spi_t instance, SPI_Regs *spi)
{
    /* DL_SPI_getPendingInterrupt/DL_SPI_disableInterrupt — NOT
     * independently confirmed, see file header comment. */
    switch (DL_SPI_getPendingInterrupt(spi)) {
    case DL_SPI_IIDX_RX: {
        uint8_t received = DL_SPI_receiveDataBlocking8(spi);
        if (s_rx_data[instance] != NULL) {
            s_rx_data[instance][s_pos[instance] - 1] = received;
        }
        if (s_pos[instance] < s_len[instance]) {
            uint8_t next_tx = (s_tx_data[instance] != NULL) ? s_tx_data[instance][s_pos[instance]] : 0xFF;
            DL_SPI_transmitDataBlocking8(spi, next_tx);
            s_pos[instance]++;
        } else {
            DL_SPI_disableInterrupt(spi, DL_SPI_INTERRUPT_RX);
            hf_gpio_write(s_active_cs[instance], true);
            s_busy[instance] = false;
            if (s_is_read[instance]) {
                if (s_rx_callback[instance] != NULL) {
                    s_rx_callback[instance](instance);
                }
            } else {
                if (s_tx_callback[instance] != NULL) {
                    s_tx_callback[instance](instance);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

#ifdef SPI0
void SPI0_IRQHandler(void)
{
    hf_spi_service_irq(0, SPI0);
}
#endif
#ifdef SPI1
void SPI1_IRQHandler(void)
{
    hf_spi_service_irq(1, SPI1);
}
#endif

#endif /* HF_ENABLE_SPI */
#endif /* TARGET_TI_MSPM0 */
