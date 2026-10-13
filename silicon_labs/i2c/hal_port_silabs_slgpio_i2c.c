/* Silicon Labs adapter for the I2C portion of hal_wrapper.h, against
 * the current `sl_i2c` driver (Simplicity SDK's "Peripheral HAL"
 * architecture — the I2C counterpart to sl_gpio in
 * hal_port_silabs_slgpio.c). Gated by TARGET_SILABS_SLGPIO, same
 * target macro the sl_gpio GPIO adapter uses — one target selects the
 * whole "current driver" family per vendor, not one macro per
 * peripheral.
 *
 * Verified platform/driver/i2c/inc/sl_i2c.h in
 * SiliconLabs/simplicity_sdk (sisdk-2025.6) directly before writing
 * this — two real surprises worth recording:
 *
 * 1. sl_i2c_send_blocking/receive_blocking/transfer take NO target-
 *    address parameter at all, unlike every other vendor here. The
 *    target ("follower", in this vendor's own terminology) address is
 *    bound to the handle via sl_i2c_set_follower_address() instead —
 *    so every function below calls that first, since our own API
 *    contract takes device_addr per call, not per handle.
 * 2. sl_i2c_init_params_t wants SCL/SDA as sl_gpio_t values directly,
 *    same "no separate IOMUX pre-muxing step at this driver layer"
 *    situation as nRF52's TWIM in hal_port_nrf52_i2c.c — EDIT the pin
 *    tables below to match your board's actual wiring.
 *
 * sl_i2c_transfer() is this driver's own native combined write-then-
 * read call — no need to hand-roll repeated-START behavior the way
 * the STM32 adapter has to.
 *
 * The non-blocking callback's SL_I2C_EVENT_STOP/IDLE values are not
 * confirmed terminal-vs-intermediate from the header alone (unlike
 * COMPLETED and the named error events, which are unambiguous) — this
 * file treats them as intermediate/ignorable. Verify against real
 * hardware before relying on this in a timing-sensitive application.
 *
 * Checked (not a gap): unlike this vendor's classic-emlib-shaped
 * adapters (GPIO, UART, classic I2C, ADC, Timer), which turned out to
 * need their own explicit CMU_ClockEnable()/init call, sl_i2c_init()
 * already enables its own I2C bus clock internally via
 * sl_clock_manager_enable_bus_clock() — confirmed against
 * platform/driver/i2c/src/sl_i2c.c in SiliconLabs/simplicity_sdk. No
 * fix needed here.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_I2C

#include "sl_i2c.h"

#ifdef I2C0
#define HF_SLI2C_BASE_0 I2C0
#else
#define HF_SLI2C_BASE_0 NULL
#endif
#ifdef I2C1
#define HF_SLI2C_BASE_1 I2C1
#else
#define HF_SLI2C_BASE_1 NULL
#endif

static I2C_TypeDef * const s_i2c_base_table[] = { HF_SLI2C_BASE_0, HF_SLI2C_BASE_1 };
#define HF_SLI2C_COUNT (sizeof(s_i2c_base_table) / sizeof(s_i2c_base_table[0]))

/* Large-but-finite: sl_i2c_send_blocking/receive_blocking/transfer
 * all require a numeric millisecond timeout (unlike, say, STM32 HAL's
 * HAL_MAX_DELAY), so there's no "block forever" option to pass
 * through even though hf_i2c_write/read/write_read's own contract has
 * no timeout concept. One second comfortably covers any real I2C
 * transaction at Standard/Fast/Fast-Plus speed. */
#define HF_SLI2C_BLOCKING_TIMEOUT_MS 1000U

/* EDIT ME: SCL/SDA pins per instance — see file header comment. */
static const sl_gpio_t s_scl_pins[HF_SLI2C_COUNT] = {
    { .port = SL_GPIO_PORT_A, .pin = 1 }, /* I2C0 SCL — EDIT ME */
    { .port = SL_GPIO_PORT_A, .pin = 3 }, /* I2C1 SCL — EDIT ME */
};
static const sl_gpio_t s_sda_pins[HF_SLI2C_COUNT] = {
    { .port = SL_GPIO_PORT_A, .pin = 0 }, /* I2C0 SDA — EDIT ME */
    { .port = SL_GPIO_PORT_A, .pin = 2 }, /* I2C1 SDA — EDIT ME */
};

static sl_i2c_handle_t s_handles[HF_SLI2C_COUNT];
static hf_i2c_tx_callback_t s_tx_callback[HF_SLI2C_COUNT];
static hf_i2c_rx_callback_t s_rx_callback[HF_SLI2C_COUNT];

static bool hf_i2c_valid(hf_i2c_t instance)
{
    return instance < HF_SLI2C_COUNT && s_i2c_base_table[instance] != NULL;
}

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }

    sl_i2c_init_params_t params = {0};
    params.i2c_base_addr = s_i2c_base_table[instance];
    params.operating_mode = SL_I2C_LEADER_MODE;
    params.follower_address = 0; /* set per-call via sl_i2c_set_follower_address instead */
    params.scl_gpio = s_scl_pins[instance];
    params.sda_gpio = s_sda_pins[instance];
    params.fifo_threshold = NULL; /* accept the driver's default */

    if (clock_hz >= 1000000) {
        params.freq_mode = SL_I2C_FREQ_FASTPLUS_MODE;
    } else if (clock_hz >= 400000) {
        params.freq_mode = SL_I2C_FREQ_FAST_MODE;
    } else {
        params.freq_mode = SL_I2C_FREQ_STANDARD_MODE;
    }

    return sl_i2c_init(&params, &s_handles[instance]) == SL_STATUS_OK;
}

bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }
    if (sl_i2c_set_follower_address(s_handles[instance], device_addr) != SL_STATUS_OK) {
        return false;
    }
    return sl_i2c_send_blocking(s_handles[instance], data, (uint16_t)len,
                                 HF_SLI2C_BLOCKING_TIMEOUT_MS) == SL_STATUS_OK;
}

bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }
    if (sl_i2c_set_follower_address(s_handles[instance], device_addr) != SL_STATUS_OK) {
        return false;
    }
    return sl_i2c_receive_blocking(s_handles[instance], data, (uint16_t)len,
                                    HF_SLI2C_BLOCKING_TIMEOUT_MS) == SL_STATUS_OK;
}

bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }
    if (sl_i2c_set_follower_address(s_handles[instance], device_addr) != SL_STATUS_OK) {
        return false;
    }
    return sl_i2c_transfer(s_handles[instance], tx_data, (uint16_t)tx_len,
                            rx_data, (uint16_t)rx_len) == SL_STATUS_OK;
}

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback)
{
    if (instance < HF_SLI2C_COUNT) {
        s_tx_callback[instance] = callback;
    }
}

void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback)
{
    if (instance < HF_SLI2C_COUNT) {
        s_rx_callback[instance] = callback;
    }
}

static bool hf_i2c_event_is_terminal(sl_i2c_event_t event, bool *out_success)
{
    switch (event) {
    case SL_I2C_EVENT_COMPLETED:
        *out_success = true;
        return true;
    case SL_I2C_EVENT_NACK_RECEIVED:
    case SL_I2C_EVENT_ARBITRATION_LOST:
    case SL_I2C_EVENT_BUS_ERROR:
    case SL_I2C_EVENT_SW_FAULT:
    case SL_I2C_EVENT_INVALID_ADDR:
    case SL_I2C_EVENT_TIMEOUT:
        *out_success = false;
        return true;
    default: /* IN_PROGRESS, STOP, IDLE — treated as intermediate */
        return false;
    }
}

static sl_status_t hf_i2c_tx_trampoline(sl_i2c_event_t event, void *context)
{
    hf_i2c_t instance = (hf_i2c_t)(uintptr_t)context;
    bool success;
    if (instance < HF_SLI2C_COUNT && s_tx_callback[instance] != NULL
        && hf_i2c_event_is_terminal(event, &success)) {
        s_tx_callback[instance](instance, success);
    }
    return SL_STATUS_OK;
}

static sl_status_t hf_i2c_rx_trampoline(sl_i2c_event_t event, void *context)
{
    hf_i2c_t instance = (hf_i2c_t)(uintptr_t)context;
    bool success;
    if (instance < HF_SLI2C_COUNT && s_rx_callback[instance] != NULL
        && hf_i2c_event_is_terminal(event, &success)) {
        s_rx_callback[instance](instance, success);
    }
    return SL_STATUS_OK;
}

bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }
    if (sl_i2c_set_follower_address(s_handles[instance], device_addr) != SL_STATUS_OK) {
        return false;
    }
    return sl_i2c_send_non_blocking(s_handles[instance], data, (uint16_t)len,
                                     hf_i2c_tx_trampoline,
                                     (void *)(uintptr_t)instance) == SL_STATUS_OK;
}

bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len)
{
    if (!hf_i2c_valid(instance)) {
        return false;
    }
    if (sl_i2c_set_follower_address(s_handles[instance], device_addr) != SL_STATUS_OK) {
        return false;
    }
    return sl_i2c_receive_non_blocking(s_handles[instance], data, (uint16_t)len,
                                        hf_i2c_rx_trampoline,
                                        (void *)(uintptr_t)instance) == SL_STATUS_OK;
}

#endif /* HF_ENABLE_I2C */
#endif /* TARGET_SILABS_SLGPIO */
