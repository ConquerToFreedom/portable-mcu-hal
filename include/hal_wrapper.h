#ifndef MCU_HAL_WRAPPER_H
#define MCU_HAL_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#include "hal_wrapper_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Target selection — exactly one must be defined by the IDE project  */
/* preprocessor settings. Silent misconfiguration is worse than a     */
/* build error, so this is intentionally loud.                        */
/*                                                                      */
/* TARGET_SILABS and TARGET_SILABS_SLGPIO are two different targets,   */
/* not two names for the same one: they select two genuinely           */
/* different, incompatible Silicon Labs GPIO APIs (classic emlib vs.   */
/* the current sl_gpio driver) — see docs/gpio-api-reference.md.       */
/* TARGET_NRF52 covers both nRF5 SDK and nRF Connect SDK unchanged,     */
/* since both share the same underlying nrfx nrf_gpio.h — no second    */
/* nRF target exists because there's no second API to select.          */
/* ------------------------------------------------------------------ */
#if !defined(TARGET_STM32) && !defined(TARGET_NRF52) && \
    !defined(TARGET_SILABS) && !defined(TARGET_SILABS_SLGPIO) && \
    !defined(TARGET_TI_MSPM0)
#error "Define exactly one of TARGET_STM32 / TARGET_NRF52 / TARGET_SILABS / TARGET_SILABS_SLGPIO / TARGET_TI_MSPM0 in your IDE's preprocessor settings."
#endif

/* ------------------------------------------------------------------ */
/* GPIO — frozen (Stage 1)                                            */
/* ------------------------------------------------------------------ */

/* Opaque pin identifier: (port << 5) | pin. Zero RAM cost — this is a
 * scalar passed by value, decoded to a register address inside each
 * vendor adapter via a small const lookup table in flash. See
 * docs/gpio-api-reference.md for the exact decode pattern.
 *
 * Prefix is "hf_"/"HF_" (not "hal_"/"HAL_") specifically so it never
 * visually collides with a vendor SDK's own naming — STM32's own SDK
 * is literally called "STM32 HAL", so hal_gpio_write() sitting next to
 * HAL_GPIO_WritePin() in the same adapter file was a real source of
 * mix-ups. */
typedef uint32_t hf_gpio_t;

#define HF_PIN(port, num)  ((hf_gpio_t)(((uint32_t)(port) << 5) | ((num) & 0x1F)))

typedef enum {
    HF_GPIO_INPUT,
    HF_GPIO_INPUT_PULLUP,
    HF_GPIO_INPUT_PULLDOWN,
    HF_GPIO_OUTPUT_PUSHPULL,
    HF_GPIO_OUTPUT_OPENDRAIN
} hf_gpio_mode_t;

bool hf_gpio_init(hf_gpio_t pin, hf_gpio_mode_t mode);
void hf_gpio_write(hf_gpio_t pin, bool state);
bool hf_gpio_read(hf_gpio_t pin);
void hf_gpio_toggle(hf_gpio_t pin);

/* ------------------------------------------------------------------ */
/* UART — frozen (Stage 4)                                             */
/* ------------------------------------------------------------------ */

/* Opaque instance identifier: 0, 1, 2, ... selects the vendor's Nth
 * UART/USART peripheral. This is the same "small index, adapter
 * decodes via a lookup table" pattern as hf_gpio_t's port field — but
 * unlike GPIO's port+pin, there's no cross-vendor numbering to borrow
 * for familiarity here: STM32's own USART1/2/3 + UART4/5 + USART6
 * naming isn't even contiguous on one vendor, let alone across four.
 * See docs/architecture.md's UART section for the full rationale.
 *
 * Does NOT encode which physical pins are TX/RX. Pin muxing for the
 * chosen instance must already be configured by the customer's own
 * vendor tool (CubeMX/SysConfig/pin tool) before hf_uart_init() runs —
 * the same division of responsibility GPIO already has on TI MSPM0
 * (see the pull/open-drain asymmetry table in docs/architecture.md).
 *
 * Fixed at 8 data bits, no parity, 1 stop bit, no flow control for v1
 * — not configurable. Covers the overwhelming majority of debug/echo
 * use cases; revisit only if a real need for 7E1/9-bit/RTS-CTS shows
 * up (see docs/development-plan.md backlog). */
typedef uint8_t hf_uart_t;

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate);

/* Blocks until all `len` bytes are sent. No timeout parameter: with no
 * hardware flow control in v1, TX has nothing external to stall on
 * indefinitely, so a timeout isn't needed to keep this safe — and
 * omitting it keeps UART from taking on a dependency on os_port.h's
 * (not yet designed) timing primitives before either one needs to.
 * Silently does nothing for an invalid `instance`, same convention as
 * hf_gpio_write. */
void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len);

/* Never blocks. Copies up to `len` bytes already received into `data`
 * and returns how many (0 if none are available yet, or if `instance`
 * is invalid). A caller that wants to wait for data loops on this
 * itself — this API stays timeout-free by design, same reasoning as
 * hf_uart_write.
 *
 * This is backed by a background receive path (interrupt- or
 * DMA-driven, per adapter — see docs/architecture.md) that continuously
 * drains the hardware into a small internal buffer regardless of
 * whether this function is being called; hf_uart_read() only drains
 * *that* buffer. It has to work this way: real UART hardware FIFOs are
 * only a few bytes deep, so pure on-demand polling of the hardware
 * itself would drop bytes the moment the caller's main loop is busy
 * for more than a few byte-times. */
uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len);

/* ------------------------------------------------------------------ */
/* UART — non-blocking (interrupt/DMA-driven) tier, frozen (Stage 4)   */
/* ------------------------------------------------------------------ */

/* Fires from interrupt context once a hf_uart_write_async() transfer
 * has fully left the peripheral. */
typedef void (*hf_uart_tx_callback_t)(hf_uart_t instance);

/* Fires from interrupt context whenever new bytes land in the
 * background receive buffer described above — not necessarily once
 * per byte (that granularity is whatever the adapter's own interrupt/
 * DMA implementation uses). This is a "there's more to read now"
 * notification, not a data handoff: call hf_uart_read() to actually
 * retrieve it, same as you would from a polling loop. */
typedef void (*hf_uart_rx_callback_t)(hf_uart_t instance);

/* Registers (or clears, with NULL) the completion/notification
 * callback for an instance. Calling either of these is optional —
 * hf_uart_write()/hf_uart_read() work fine with no callback ever
 * registered; this tier exists for callers who specifically want to
 * avoid blocking on TX or polling for RX. */
void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback);
void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback);

/* Starts sending `len` bytes from `data` without blocking; returns
 * immediately. `data` must stay valid and unmodified until the
 * registered TX callback fires (or, with none registered, until
 * you've otherwise confirmed the transfer finished) — this hands the
 * buffer to the adapter's interrupt/DMA path, it does not copy it.
 * Returns false if a transfer is already in flight on this instance
 * or `instance` is invalid — never starts a second overlapping
 * transfer silently. Returns true once the transfer has been handed
 * off (not once it's complete — that's what the callback is for). */
bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len);

/* ------------------------------------------------------------------ */
/* I2C — frozen (Stage 4)                                              */
/* ------------------------------------------------------------------ */

/* Opaque instance identifier: 0, 1, 2, ... selects the vendor's Nth
 * I2C/TWI(M) peripheral. Same lookup-table pattern as hf_gpio_t's port
 * and hf_uart_t's instance — again no cross-vendor numbering to borrow
 * (nRF52's TWIM0/1 vs. STM32's I2C1/2/3 vs. TI's I2C0/1 share nothing).
 * Pin muxing is out of scope here too, same reasoning as UART's. */
typedef uint8_t hf_i2c_t;

/* Typical values: 100000 (Standard Mode) or 400000 (Fast Mode). Fast
 * Mode Plus (1000000) and High Speed Mode are backlog, not v1 — most
 * sensors/EEPROMs used from a "thin wrapper" project target 100k/400k. */
bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz);

/* 7-bit addressing only in v1 — the vast majority of I2C devices.
 * 10-bit addressing is backlog (see docs/development-plan.md).
 *
 * Unlike UART's read/write, these return bool (success/fail), not a
 * byte count — deliberately: I2C is transactional, not a stream. A
 * transfer either completes in full or is NACK'd/errors partway
 * through; there's no meaningful "partial success" byte count to
 * report the way a UART stream has. `false` covers both an invalid
 * `instance` and a bus-level failure (NACK, arbitration loss, etc.) —
 * the adapter doesn't distinguish which in the return value, since
 * "did it work" is what the blocking tier is for; use the async tier's
 * `success` callback parameter if you need to tell those apart. */
bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len);
bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len);

/* Write `tx_len` bytes, then read `rx_len` bytes, from the same
 * `device_addr` in one transaction (repeated START between the two
 * phases, not a STOP+START) — the standard "write register address,
 * then read its value" pattern nearly every I2C sensor/EEPROM uses.
 * Deliberately generalized rather than baking in a fixed register-
 * address width (STM32 HAL's Mem_Read/Write hardcodes 8-bit/16-bit
 * MemAddSize) — the register address is just whatever the caller puts
 * in `tx_data`, so any addressing width the device uses works with no
 * extra parameter. */
bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len);

/* ------------------------------------------------------------------ */
/* I2C — non-blocking (interrupt/DMA-driven) tier, frozen (Stage 4)    */
/* ------------------------------------------------------------------ */

/* Fires from interrupt context once a hf_i2c_write_async()/
 * hf_i2c_read_async() transfer finishes — `success` is false on a
 * NACK or other bus error, same failure modes the blocking calls fold
 * into their own bool return. Unlike hf_uart_tx/rx_callback_t, I2C's
 * callbacks carry a success flag on purpose: a NACK'd transaction
 * (wrong address, device busy/absent) is routine, expected operation
 * for I2C — e.g. bus probing/scanning relies on it — not a rare
 * hardware fault the way a UART framing error would be, so it isn't
 * something this API can afford to leave unreported. */
typedef void (*hf_i2c_tx_callback_t)(hf_i2c_t instance, bool success);
typedef void (*hf_i2c_rx_callback_t)(hf_i2c_t instance, bool success);

void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback);
void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback);

/* Starts a write to `device_addr` without blocking; returns
 * immediately. `data` must stay valid until the registered TX
 * callback fires. Returns false if a transfer is already in flight on
 * this instance or `instance` is invalid; true once handed off (not
 * once complete). Same "no DMA channel selection exposed, adapter
 * picks per vendor" reasoning as hf_uart_write_async. */
bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len);

/* Starts a read from `device_addr` without blocking; returns
 * immediately. `data` must stay valid until the registered RX
 * callback fires, at which point exactly `len` bytes are in it if
 * `success` is true. Unlike hf_uart_rx_callback_t's "there's more,
 * go call hf_uart_read()" notification, this signals "your `len`
 * bytes are now in `data`" directly — I2C reads are always a
 * known-length transaction, never an open-ended stream, so there's no
 * separate drain call the way UART has. */
bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len);

/* hf_i2c_write_read's async equivalent isn't in v1 — the blocking
 * version already covers the common "read a sensor register" case,
 * and combining a two-phase transfer into the single tx/rx-callback
 * model above cleanly needs more design thought than this pass
 * warrants. Backlog item, see docs/development-plan.md. */

/* ------------------------------------------------------------------ */
/* SPI — frozen (Stage 4)                                              */
/* ------------------------------------------------------------------ */

/* SPI was originally out of scope for month 1 (see the decisions log
 * in docs/architecture.md — deferred alongside ADC, "most vendor-
 * specific quirks live there") and is pulled forward here at the
 * user's explicit request, same re-scoping this project has already
 * done more than once.
 *
 * Opaque instance identifier: 0, 1, 2, ... selects the vendor's Nth
 * SPI peripheral — same lookup-table pattern as hf_i2c_t/hf_uart_t.
 * Pin muxing (SCK/MOSI/MISO) is out of scope here too, same reasoning
 * as UART/I2C's.
 *
 * Master-only, same scope decision as I2C — this project doesn't
 * target SPI slave/peripheral mode. */
typedef uint8_t hf_spi_t;

/* CPOL/CPHA, using SPI's own universal mode numbering (0-3) rather
 * than inventing a new one — this is already a vendor-agnostic,
 * industry-standard convention, the same "borrow the familiar
 * pattern" reasoning as hf_gpio_t's encoding matching Nordic's
 * NRF_GPIO_PIN_MAP. */
typedef enum {
    HF_SPI_MODE_0, /* CPOL=0, CPHA=0 */
    HF_SPI_MODE_1, /* CPOL=0, CPHA=1 */
    HF_SPI_MODE_2, /* CPOL=1, CPHA=0 */
    HF_SPI_MODE_3  /* CPOL=1, CPHA=1 */
} hf_spi_mode_t;

/* Fixed at MSB-first bit order for v1 — not configurable, same
 * "cover the overwhelming majority" reasoning as UART's fixed 8N1.
 * LSB-first is backlog (see docs/development-plan.md). */
bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode);

/* Chip-select is deliberately NOT a new peripheral concept — SPI
 * doesn't address devices the way I2C does, so every call here takes
 * `cs_pin`, a plain hf_gpio_t the caller already configured with
 * hf_gpio_init(cs_pin, HF_GPIO_OUTPUT_PUSHPULL). Each call asserts it
 * (drives low), runs the transfer, then deasserts it (drives high) —
 * active-low, not configurable in v1, since that's the overwhelming
 * SPI convention; active-high is backlog. This composes the
 * already-frozen GPIO API directly rather than adding a second way to
 * drive a pin. */

/* Write-only: clocks `len` bytes from `data` out on MOSI, discarding
 * whatever comes back on MISO. Returns bool like I2C's blocking calls,
 * not a byte count — SPI is a strictly clocked shift with no
 * meaningful partial-transfer outcome the way a UART stream has, and
 * unlike I2C there isn't even a NACK-style bus failure mode; `false`
 * here only ever means an invalid `instance`/`cs_pin`. */
bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len);

/* Read-only: clocks `len` dummy bytes out on MOSI (the value is
 * implementation-defined — most SPI devices ignore MOSI during a
 * read) while capturing what comes back on MISO into `data`. */
bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len);

/* Full-duplex: SPI's actual native operation. `tx_data` and `rx_data`
 * are clocked simultaneously, not sequentially the way
 * hf_i2c_write_read's two phases are — every bit shifted out on MOSI
 * has a bit shifted in on MISO at the same clock edge. This is why
 * hf_spi_write/read above are described as clocking "dummy"/"discarded"
 * bytes rather than as a genuinely separate mode: underneath, every
 * SPI transfer is this function, just with one side unused. */
bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

/* ------------------------------------------------------------------ */
/* SPI — non-blocking (interrupt/DMA-driven) tier, frozen (Stage 4)    */
/* ------------------------------------------------------------------ */

/* Parameterless, like hf_uart_tx/rx_callback_t, not success-carrying
 * like hf_i2c_tx/rx_callback_t: SPI has no NACK-equivalent bus-level
 * failure signal the way I2C does, so there's nothing meaningful for
 * a success flag to report beyond what an invalid instance/cs_pin
 * already rejects synchronously at the call site. */
typedef void (*hf_spi_tx_callback_t)(hf_spi_t instance);
typedef void (*hf_spi_rx_callback_t)(hf_spi_t instance);

void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback);
void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback);

/* Same shape and guarantees as hf_uart_write_async/hf_i2c_write_async:
 * returns immediately, `data` must stay valid until the callback
 * fires, `cs_pin` stays asserted for the whole transfer and is
 * released when it completes, returns false if a transfer is already
 * in flight on this instance or the instance/pin is invalid. */
bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len);
bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len);

/* hf_spi_transfer's async equivalent isn't in v1 — same reasoning as
 * hf_i2c_write_read_async's deferral: the blocking version already
 * covers full-duplex use, and a genuinely simultaneous async
 * completion doesn't fit the separate tx/rx-callback model above any
 * more cleanly than I2C's two-phase transfer did. Backlog item, see
 * docs/development-plan.md. */

/* ------------------------------------------------------------------ */
/* ADC — frozen (Stage 4)                                              */
/* ------------------------------------------------------------------ */

/* ADC was originally out of month-1 scope (grouped with SPI in the
 * decisions log in docs/architecture.md, "most vendor-specific quirks
 * live there") and is pulled forward here at the user's explicit
 * request, same re-scoping already applied to GPIO and SPI.
 *
 * Opaque instance identifier: 0, 1, 2, ... selects the vendor's Nth
 * ADC peripheral — same lookup-table pattern as hf_i2c_t/hf_uart_t/
 * hf_spi_t. Channel-to-pin routing is a fixed silicon mapping on every
 * one of these four vendors, not a user-selectable mux the way digital
 * GPIO alternate functions are, so — same reasoning as hf_i2c_t's
 * device_addr — `hf_adc_channel_t` is just the vendor's own channel
 * number; remapping it to a pin is out of scope here. */
typedef uint8_t hf_adc_t;
typedef uint8_t hf_adc_channel_t;

/* Fixed at 12-bit resolution for v1 — the widest common denominator
 * across all four vendors (STM32's ADC resets to 12-bit and supports
 * more; nRF52's SAADC supports up to 14-bit but is commonly run at 12;
 * Silicon Labs' classic ADC and current IADC both support 12-bit; TI's
 * ADC12 is fixed 12-bit by name), same "cover the overwhelming
 * majority, configurability is backlog" reasoning as UART's fixed 8N1.
 * Samples are returned as a plain uint16_t, 0-4095. */
bool hf_adc_init(hf_adc_t instance);

/* Single-shot blocking conversion on one channel. `bool` return, not
 * the raw value, because a conversion always either fully completes
 * or the instance/channel was invalid to begin with — the same "no
 * meaningful partial outcome" reasoning as hf_spi_write/read, not
 * I2C's NACK-driven bool. On success, `*value` holds the 0-4095
 * sample. */
bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value);

/* ------------------------------------------------------------------ */
/* ADC — non-blocking (interrupt/DMA-driven) tier, frozen (Stage 4)    */
/* ------------------------------------------------------------------ */

/* One callback, not a tx/rx pair like UART/I2C/SPI: ADC is
 * receive-only, there's no "write" side to this peripheral (driving
 * an analog output is a DAC, out of scope here). Parameterless, same
 * reasoning as hf_spi_tx/rx_callback_t: no bus-failure-style outcome
 * exists for a single conversion beyond what the synchronous `false`
 * return already rejects at the call site. */
typedef void (*hf_adc_callback_t)(hf_adc_t instance);
void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback);

/* Same shape and guarantees as hf_spi_read_async: returns immediately,
 * `value` must stay valid until the callback fires, returns false if
 * a conversion is already in flight on this instance or the
 * instance/channel is invalid. */
bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value);

/* Multi-channel scan mode and continuous/free-running conversion are
 * both backlog — v1 only covers "convert this one channel, once, per
 * call", the same discrete-operation shape every other peripheral in
 * this header uses. See docs/development-plan.md. */

/* ------------------------------------------------------------------ */
/* Timer — periodic/one-shot callback timer, frozen (Stage 4)          */
/* ------------------------------------------------------------------ */

/* Opaque instance identifier: 0, 1, 2, ... selects the vendor's Nth
 * general-purpose timer peripheral configured for this role. Same
 * lookup-table-in-flash pattern as hf_uart_t/hf_i2c_t/hf_spi_t/hf_adc_t.
 *
 * Deliberately its own, independent instance-number space from the not-
 * yet-designed hf_pwm_t, not a shared one — on nRF52 in particular,
 * periodic counting (nrfx_timer/nrfx_rtc) and PWM output (nrfx_pwm) are
 * entirely separate hardware peripherals, so there is no cross-vendor
 * "same instance number, different mode" relationship to preserve, the
 * way there might look tempting on STM32/Silicon Labs/TI where a single
 * physical timer block can be configured either way. Whether a given
 * vendor's hf_timer_t instance 0 and hf_pwm_t instance 0 end up sharing
 * the same physical timer (and therefore can't both be enabled
 * independently) is a real, vendor-specific finding to document once
 * PWM is designed — same "document it, don't hide it" treatment as
 * Silicon Labs' UART/SPI USART-sharing conflict.
 *
 * No pins are involved (unlike PWM) — same reasoning as UART/I2C/SPI's
 * instance numbering already being pin-mux-agnostic. */
typedef uint8_t hf_timer_t;

typedef enum {
    HF_TIMER_ONE_SHOT,
    HF_TIMER_PERIODIC
} hf_timer_mode_t;

/* Configures the timer's period and mode; does NOT start it — call
 * hf_timer_start() separately. This mirrors every vendor's own
 * peripheral API shape here (STM32's HAL_TIM_Base_Init vs. _Start,
 * Silicon Labs' TIMER_Init vs. a separate enable, etc.), and lets a
 * caller pause/resume a configured timer via hf_timer_stop()/
 * hf_timer_start() without recomputing prescaler/reload values on
 * every restart.
 *
 * `period_us` is the requested interval in microseconds — like
 * hf_i2c_t's clock_hz and hf_uart_t's baud_rate, translating this into
 * the vendor's actual prescaler/auto-reload register values is the
 * adapter's job, not the caller's. Microsecond resolution (rather than
 * milliseconds) is deliberate: it's finer than any of these vendors'
 * typical periodic-timer use cases need, but matches the resolution
 * PWM (backlog, see below) will need for its own period parameter, so
 * the two peripherals can share the same units convention. */
bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode);

/* Starts (or restarts, for a one-shot timer that already fired) the
 * configured timer counting. Returns false for an invalid instance or
 * one that hasn't been initialized. */
bool hf_timer_start(hf_timer_t instance);

/* Stops counting; a stopped timer can be started again via
 * hf_timer_start() without calling hf_timer_init() again, since its
 * configuration (period, mode) is retained. Silently does nothing for
 * an invalid instance, same convention as hf_gpio_write. */
void hf_timer_stop(hf_timer_t instance);

/* Fires from interrupt context every time the configured period
 * elapses (HF_TIMER_PERIODIC), or once, the single time it elapses
 * (HF_TIMER_ONE_SHOT — the timer then stops on its own; call
 * hf_timer_start() again to re-arm it). Parameterless, same reasoning
 * as hf_spi_tx/rx_callback_t and hf_adc_callback_t: a period elapsing
 * has no bus-failure-style outcome for a success flag to report. */
typedef void (*hf_timer_callback_t)(hf_timer_t instance);
void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback);

/* Deliberately NOT in v1, all backlog (see docs/development-plan.md):
 * - A blocking delay function (e.g. hf_timer_delay_ms()). Unlike
 *   UART/I2C/SPI's blocking calls, which block on a genuinely bounded
 *   hardware transfer, a delay's "how do we wait" question is really an
 *   RTOS/scheduler concern (busy-spin vs. yield-to-scheduler) — the
 *   same reasoning this header already gives for omitting UART timeout
 *   parameters pending os_port.h's (not yet designed) timing
 *   primitives. A one-shot hf_timer_t plus the caller's own spin-on-a-
 *   flag-set-by-the-callback already covers this without a dedicated
 *   function.
 * - Reading the current elapsed/remaining count mid-period.
 * - A monotonic uptime/"clock" read (an Arduino millis()-style
 *   function) — a genuinely different capability from a periodic
 *   callback timer (a passive read vs. an active callback), not
 *   designed here; revisit only if a real need surfaces. */

/* ------------------------------------------------------------------ */
/* PWM — timer + channel output, frozen (Stage 4)                      */
/* ------------------------------------------------------------------ */

/* Opaque instance identifier: (timer_instance << 2) | channel. Mirrors
 * hf_gpio_t's (port << 5) | pin encoding — same zero-RAM-cost,
 * flash-lookup-table-decoded scalar pattern every peripheral in this
 * header uses, rather than introducing a second opaque type for
 * "channel". Two bits comfortably cover every vendor researched here:
 * STM32/nRF52/TI commonly expose up to 4 output-compare channels per
 * timer, Silicon Labs' emlib TIMER exposes 3 (4 on some Series 1
 * parts).
 *
 * One physical timer's frequency is shared across every channel it
 * owns, confirmed on every vendor researched: STM32's TIM_Base
 * (Period/Prescaler in TIM_HandleTypeDef.Init, shared by all 4
 * TIM_OC_InitTypeDef channels), nRF52's nrfx_pwm_config_t.top_value
 * (one value per nrfx_pwm_t instance, up to 4 output_pins), Silicon
 * Labs classic's TIMER_TopSet() (one TOP register per TIMER_TypeDef,
 * shared by every TIMER_InitCC() channel on it), and TI's
 * DL_Timer_PWMConfig.period (one value per GPTIMER_Regs, shared by
 * every DL_Timer_setCaptureCompareValue() channel). hf_pwm_init()
 * therefore reconfigures the WHOLE timer's frequency, not just the one
 * channel passed in — calling it again for a second channel on the
 * SAME timer instance with a different freq_hz changes the first
 * channel's frequency out from under it too. This is a real hardware
 * constraint being surfaced honestly, not something this API adds
 * software to hide or enforce: pick one frequency per physical timer
 * and initialize every channel you use on it with that same freq_hz.
 *
 * Deliberately its own instance-number space from hf_timer_t, same as
 * hf_timer_t already is from PWM per that peripheral's own header
 * comment — whether the two end up sharing physical hardware is
 * genuinely vendor-specific, same "document it, don't hide it"
 * treatment as Silicon Labs' UART/SPI USART-sharing conflict:
 * - STM32: disjoint by hardware, unconditionally. hf_timer_t uses
 *   TIM6/TIM7 (basic timers — no output-compare channels or pins
 *   exist on this timer family at all), so PWM must use a
 *   general-purpose timer (TIM2/TIM3/...) regardless of instance
 *   numbering on either side.
 * - nRF52: disjoint by hardware, unconditionally. hf_timer_t uses
 *   nrfx_timer; PWM uses the entirely separate nrfx_pwm peripheral
 *   (its own EasyDMA-driven hardware block, not a mode of the counting
 *   timer).
 * - Silicon Labs classic (TARGET_SILABS): NOT disjoint by hardware —
 *   the one vendor where this can actually collide. Both hf_timer_t
 *   and hf_pwm_t are built on the same emlib TIMER peripheral family,
 *   and hf_timer_t's adapter already claims TIMER0/TIMER1. The PWM
 *   adapter must use different TIMERn instances if both peripherals
 *   are enabled on the same project — verify your part has enough
 *   TIMER blocks (some smaller EFR32 parts have only two).
 * - Silicon Labs current (TARGET_SILABS_SLGPIO): disjoint in practice.
 *   Timer's sl_sleeptimer service runs on RTCC/BURTC; PWM's sl_pwm
 *   driver runs on classic TIMER — see below for why this target still
 *   uses classic TIMER-shaped hardware for PWM specifically.
 * - TI MSPM0: disjoint by design, already reserved when Timer's
 *   adapter was written — hf_timer_t deliberately uses DL_TimerG
 *   (general-purpose), leaving DL_TimerA (advanced, PWM-capable) free
 *   for hf_pwm_t.
 *
 * No pins encoded here, same reasoning as UART/I2C/SPI/Timer's
 * instance numbering: the output-compare alternate function for
 * whichever pin channel N drives on your part is configured by your
 * own vendor tool (CubeMX/SysConfig/pin tool) before hf_pwm_init()
 * runs — this header has no way to represent that mapping generically
 * across four vendors' pin-mux schemes, the same division of
 * responsibility already established for every other peripheral here. */
typedef uint8_t hf_pwm_t;

#define HF_PWM(instance, channel)  ((hf_pwm_t)(((instance) << 2) | ((channel) & 0x3)))

/* Configures the timer's frequency (see the shared-frequency caveat
 * above) and prepares this channel's output; does not start it — call
 * hf_pwm_start() separately, same init-then-start split as hf_timer_t,
 * for the same reason: it lets a caller pause/resume via
 * hf_pwm_stop()/hf_pwm_start() without recomputing prescaler/period
 * values.
 *
 * Fixed at active-high output and edge-aligned (up-counting) mode for
 * v1 — not configurable, same "cover the overwhelming majority, revisit
 * only if a real need surfaces" reasoning as UART's fixed 8N1 and SPI's
 * fixed MSB-first. Inverted polarity (every vendor here supports it)
 * and center-aligned/up-down counting mode (TI's
 * DL_TIMER_PWM_MODE_CENTER_ALIGN, useful for motor-control EMI
 * reduction) are both backlog. */
bool hf_pwm_init(hf_pwm_t channel, uint32_t freq_hz);

/* Starts (or resumes) this channel's output at its currently configured
 * frequency and duty cycle. Returns false for an invalid channel or one
 * that hasn't been initialized. */
bool hf_pwm_start(hf_pwm_t channel);

/* Stops this channel's output; a stopped channel can be started again
 * via hf_pwm_start() without calling hf_pwm_init() again, since its
 * configuration (frequency, duty) is retained. Silently does nothing
 * for an invalid channel, same convention as hf_gpio_write. Stopping
 * one channel does not affect sibling channels sharing the same
 * physical timer — only this channel's own output-compare/enable
 * state. */
void hf_pwm_stop(hf_pwm_t channel);

/* Sets this channel's duty cycle. Callable at any time after
 * hf_pwm_init() — before the first hf_pwm_start(), or live while
 * already running — without a glitch on every vendor researched here:
 * STM32's __HAL_TIM_SET_COMPARE macro, Silicon Labs classic's buffered
 * TIMER_CompareBufSet(), TI's DL_Timer_setCaptureCompareValue(), and
 * nRF52's nrfx_pwm_sequence_update() are all designed for exactly this
 * "change it without stopping" case. This is real hardware PWM's main
 * value over bit-banging a GPIO in a loop — a fade/ramp costs one
 * register write per step, not a re-init.
 *
 * duty_millipercent: 0-100000 (i.e. 12.345% is passed as 12345), not a
 * raw compare-register value and not a plain 0-100 percent. A raw
 * compare value would tie this API to whichever vendor's timer happens
 * to have the widest counter (STM32's ARR is up to 32-bit on some
 * families, nRF52's top_value is 15-bit, Silicon Labs classic's TOP is
 * 16/24-bit depending on series, TI's period is 32-bit) — there is no
 * single number that means the same thing on all four. A plain 0-100
 * percent (which Silicon Labs' own current-SDK sl_pwm driver actually
 * uses, confirmed against platform/driver/pwm/inc/sl_pwm.h) is simple
 * but too coarse for a smooth fade — 1% steps are visible to the eye on
 * an LED. Milli-percent gives fractional-percent precision without
 * floats; every adapter converts it to whatever raw compare-value ratio
 * its own timer's configured period actually wants.
 *
 * Silicon Labs' current SDK (TARGET_SILABS_SLGPIO) is the one vendor
 * with a real, permanent resolution loss here: sl_pwm_set_duty_cycle()
 * itself only takes a plain uint8_t percent (0-100), so
 * duty_millipercent is rounded to the nearest whole percent on that
 * target specifically — flagged, not hidden, the same treatment as
 * that SDK generation's sl_sleeptimer millisecond-rounding finding for
 * Timer. This is also why hf_pwm_t's current-SDK adapter uses sl_pwm
 * (a thin convenience driver confirmed to sit directly on top of
 * classic emlib TIMER — sl_pwm_instance_t literally holds a
 * TIMER_TypeDef* — not a genuinely different peripheral the way
 * sl_gpio/sl_i2c are over classic GPIO/I2C) rather than bypassing it
 * for raw TIMER_InitCC() calls: sl_pwm is a real, current-generation
 * driver Silicon Labs ships and documents, so this project uses it and
 * documents its one real limitation, the same choice already made for
 * this vendor's UART/SPI adapters' shared-driver situation. */
bool hf_pwm_set_duty(hf_pwm_t channel, uint32_t duty_millipercent);

/* Deliberately NOT in v1, all backlog (see docs/development-plan.md):
 * - Inverted output polarity and center-aligned counting mode (see
 *   hf_pwm_init()'s own comment above).
 * - Reading back the currently configured duty cycle or frequency.
 * - Complementary/dead-time-inserted channel pairs (H-bridge
 *   motor-control driving) — a genuinely more involved feature on
 *   every vendor here (STM32's TIM1/TIM8 advanced-control timers,
 *   TI's DL_TimerA dead-band generator), out of scope for this
 *   project's "thin wrapper" ambitions. */

/* ------------------------------------------------------------------ */
/* Flash — internal (on-chip) NVM read/erase/write, frozen (Stage 5)   */
/* ------------------------------------------------------------------ */

/* Unlike every peripheral above, there is no hf_flash_t opaque
 * instance identifier and no HF_FLASH(...) encoding macro: internal
 * flash is a single, singular region per chip on every one of these
 * four vendors, not an "Nth peripheral" to select the way
 * GPIO/UART/I2C/SPI/ADC/Timer/PWM all are. The API below is addressed
 * directly by raw chip memory address instead of a scalar+lookup-table
 * handle — there is nothing for a lookup table to decode.
 *
 * THE ONE RULE THAT MATTERS, surfaced loudly rather than hidden the
 * way PWM's shared-frequency caveat is: every function below uses
 * each vendor's normal, flash-resident blocking erase/program call —
 * NOT a RAM-executed variant. On every vendor researched here,
 * erasing or programming the flash bank your CPU is currently
 * executing code from (or reading as const data) is unsafe with that
 * normal call: TI's driverlib ships separate "FromRAM"/RAM-executed
 * erase and program functions specifically because the blocking,
 * flash-resident ones can hang or fault the part when they target
 * their own bank; ST's own FLASH_EraseProgram example warns that
 * erasing/writing the code you are currently running can crash the
 * device; Silicon Labs' MSC write functions are themselves placed in
 * RAM by the SDK's build system on affected parts for this exact
 * reason (SL_RAMFUNC), which this project's classic adapter relies on
 * rather than re-implements. This project does NOT implement the
 * RAM-executed variants in v1 — backlog, see docs/development-plan.md.
 * Confine hf_flash_erase()/hf_flash_write() to a region your own
 * linker script has reserved for data storage and never placed code,
 * vector tables, or currently-read const data in. This is a hardware
 * constraint being surfaced honestly, not something this API adds
 * software to hide or enforce, the same treatment already given to
 * PWM's shared-timer-frequency caveat and Silicon Labs' UART/SPI
 * USART-sharing conflict.
 *
 * hf_flash_init() performs one-time setup. On most vendors this is a
 * no-op that only returns true — the exception is Silicon Labs
 * classic, where emlib's own MSC_Init() must run before any MSC
 * erase/write call, the same "a real per-vendor requirement, not
 * invented busywork" reasoning as hf_i2c_init()/hf_spi_init() already
 * have. Harmless, and required, to call even where it's a no-op —
 * every other peripheral in this header has an init() call too, and
 * this keeps hf_flash_t consistent with that rather than being the
 * one exception. */
bool hf_flash_init(void);

/* Returns the erase granularity, in bytes, of whichever erase unit
 * (page/sector) contains `address`. Deliberately takes an address
 * parameter rather than being a zero-argument constant getter: STM32's
 * sector-based families (F2/F4/F7/H7) have NON-UNIFORM sector sizes —
 * the F4 family this project's other STM32 adapters use as their
 * reference point has 16 KB sectors 0-3, a 64 KB sector 4, then 128 KB
 * sectors 5 and up, all in the same flash bank. A zero-argument
 * constant would silently lie on exactly the vendor this project
 * already treats as its most-varied one (see hf_adc_t's ADC_REGULAR_RANK_1
 * comment, hf_timer_t's TIM6/TIM7 availability comment). nRF52
 * (4096 bytes, uniform), Silicon Labs classic (varies by part's total
 * flash size — EDIT the constant in that adapter for your part, same
 * "trim the table to match your part" precedent as this vendor's
 * TIMER-count caveat), and TI MSPM0 (uniform per part, exact value
 * NOT independently confirmed against an installed SDK — flagged, not
 * guessed, in that adapter's own header) all ignore which address is
 * passed and return one constant regardless. */
uint32_t hf_flash_erase_size(uint32_t address);

/* Returns the minimum alignment, in bytes, required for BOTH the
 * address and the data length passed to hf_flash_write(): 4 on
 * STM32 (F4 family reference: FLASH_TYPEPROGRAM_WORD — other families'
 * L4/G4/G0's FLASH_TYPEPROGRAM_DOUBLEWORD or H7's 256-bit flash word
 * need a different constant, same per-family substitution already
 * flagged throughout this vendor's adapters), nRF52 (nrfx_nvmc_words_write
 * requires a word-aligned address and writes whole 32-bit words), and
 * Silicon Labs classic (MSC_WriteWord requires a word-aligned address
 * and a byte count divisible by 4); 8 on TI MSPM0, whose flash word is
 * 64 bits wide. Takes no address parameter, unlike hf_flash_erase_size
 * — this is a property of the vendor's write-data-path width, not of
 * which erase unit an address falls in. */
uint32_t hf_flash_write_align(void);

/* Erases the whole erase unit(s) covering [address, address+length).
 * Both `address` and `length` must be exact multiples of
 * hf_flash_erase_size(address) — returns false otherwise, rather than
 * silently rounding up and erasing more than asked. This is the same
 * "no hidden side effects" rule hf_flash_write() below follows for
 * auto-erase: a caller that wants more than one erase unit erased
 * calls this once with a `length` spanning all of them (the adapter
 * loops internally if the vendor's own erase call only takes one unit
 * at a time, e.g. STM32's HAL_FLASHEx_Erase or Silicon Labs'
 * MSC_ErasePage), not once per unit. Erased flash reads as all-1-bits
 * (0xFF per byte) on every vendor here — this is why hf_flash_write()
 * can only clear bits, never set them, without erasing first. */
bool hf_flash_erase(uint32_t address, uint32_t length);

/* Programs `length` bytes from `data` starting at `address`. Both
 * must be exact multiples of hf_flash_write_align() — returns false
 * otherwise. Deliberately does NOT erase first: flash can only clear
 * bits (1->0), never set them, so writing into not-already-erased
 * flash silently produces the bitwise AND of the old and new data, not
 * the new data — a caller must call hf_flash_erase() itself first.
 * This mirrors hf_pwm_init()'s shared-frequency caveat and
 * hf_i2c_write_read()'s "no default register-address width" choice:
 * surfacing a real hardware constraint rather than adding software to
 * silently auto-erase (and in doing so destroy whatever else shares
 * that erase unit beyond this call's own `length`, if the caller only
 * meant to update part of it). */
bool hf_flash_write(uint32_t address, uint8_t *data, uint32_t length);

/* Plain read of already-programmed (or erased, reading as 0xFF) flash
 * into `data`. No alignment requirement on `address` or `length` —
 * every vendor here maps internal flash directly into the CPU's
 * readable address space, so this is a byte-granular memory copy, not
 * a peripheral transaction the way hf_flash_erase/write are. Provided
 * for API completeness/symmetry rather than necessity: a caller could
 * equally do `memcpy(data, (const void *)address, length)` directly on
 * every one of these four vendors, since none of them require going
 * through a special read path the way some external SPI/QSPI flash
 * parts would. */
bool hf_flash_read(uint32_t address, uint8_t *data, uint32_t length);

/* Deliberately NOT in v1, all backlog (see docs/development-plan.md):
 * - RAM-executed erase/program variants (see the header comment
 *   above) — needed only by a caller that genuinely must modify the
 *   flash bank it is currently executing from, a materially more
 *   involved feature (linker-script RAM placement, not just a
 *   different driverlib call) than this project's other backlog
 *   deferrals.
 * - An async/callback-driven tier, unlike every other peripheral in
 *   this header — flash erase/program operations are rare, slow
 *   (milliseconds, not microseconds), one-at-a-time operations in
 *   every real use case this project targets (writing config/
 *   calibration data, not a high-throughput data log), so the
 *   complexity of a non-blocking tier was judged not worth it for v1,
 *   unlike UART/I2C/SPI/ADC's genuinely latency-sensitive transfers.
 * - Wear-leveling, a filesystem, or a key-value store on top of raw
 *   flash (the problem Silicon Labs' own NVM3 and similar vendor
 *   libraries solve) — deliberately out of scope for a "thin wrapper"
 *   over the raw peripheral, the same scope boundary already drawn
 *   around every other peripheral here. */

#ifdef __cplusplus
}
#endif

#endif /* MCU_HAL_WRAPPER_H */
