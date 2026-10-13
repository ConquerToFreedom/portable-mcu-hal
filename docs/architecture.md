# Architecture — mcu-hal (working name)

Status: planning locked, implementation not yet started.
Scope for v1: ARM Cortex-M vendor portability only. ESP32/Xtensa explicitly
out of scope — different toolchain, calling convention, and startup/vector
table shape makes wrapping it low-value; someone leaving ESP32 rewrites
regardless.

## Decision: not a driver library, not an RTOS

This project does not reimplement vendor drivers (that's Zephyr's job, and
Zephyr already does it well — see "Why not just use X" below). It is a thin
contract layer that sits between the customer's business logic and the
official vendor SDK, which the customer downloads themselves from
ST/Nordic/Silicon Labs/TI.

```
Layer 3 — customer business logic (written only against Layer 1)
Layer 1 — mcu-hal public API (hal_wrapper.h) — the contract, no logic
Layer 2 — per-vendor adapter (<vendor>/<peripheral>/hal_port_<vendor>.c) — maps Layer 1 to Layer 0
Layer 0 — official vendor SDK (STM32Cube / nRF SDK / Gecko SDK / MSPM0 driverlib)
          — unchanged, downloaded by the customer, never modified by us
```

RTOS is handled the same way, kept separate from the peripheral HAL entirely:
a small `os_port.h` contract (`os_delay_ms`, `os_mutex_lock/unlock`, etc.)
that the customer or a bundled binding implements once for bare-metal,
FreeRTOS, or whatever they already run. We never link against a kernel
ourselves — this is the actual differentiator over Zephyr, which forces
adoption of its own kernel + build system to get vendor-portable drivers.

## Why not just use X

- **Zephyr**: does exactly this, but only if you adopt its kernel, its
  build system (CMake + devicetree + west), and its scheduler. Real
  friction for teams on bare-metal or an existing FreeRTOS codebase who
  don't want a full framework migration just to change silicon vendor.
  (Zephyr adoption is real and growing — a recent Linux Foundation survey
  found the majority of *surveyed* orgs already using it in commercial
  products — so "nobody uses Zephyr" is not a safe claim to build
  positioning on. The gap we fill is narrower: portable *and*
  kernel-optional, which Zephyr doesn't offer.)
- **embedded-hal (Rust)**: correctly OS-agnostic, proves the trait-based
  approach works without owning a kernel — but Rust-embedded toolchain
  adoption is still a minority of the market we're targeting (teams
  already shipping C on these vendors' official SDKs).
- **CMSIS-Core / CMSIS-Driver**: CMSIS-Core's compiler abstraction
  (`__STATIC_INLINE`, `__WEAK`, etc.) is reused directly rather than
  reinvented — every Cortex-M vendor SDK already ships it, so toolchain
  portability across STM32CubeIDE/IAR/Keil/MCUXpresso comes for free.
  CMSIS-Driver (standardized peripheral API spec) has inconsistent
  vendor coverage — adapt to it where it exists, don't depend on it.

## Public API surface (v1, GPIO only — first peripheral)

Opaque scalar pin identifier, not a struct, not a handle:

```c
typedef uint32_t hf_gpio_t;   // encoded (port << 5) | pin
```

Prefix is `hf_`/`HF_`, not `hal_`/`HAL_` — deliberately, since STM32's own
SDK is literally called "STM32 HAL". A shared `hal_` prefix on both our
code and STM32's own `HAL_GPIO_WritePin()` et al. made the two easy to
mix up inside the same adapter file.

Rationale: zero RAM cost. GPIO is stateless — no per-pin object needed.
The only storage anywhere is a small `const` port-base lookup table in
**flash** (not RAM) inside each vendor adapter, used to decode the port
index back into a real register base pointer. The scalar is passed in a
CPU register on every call (ARM AAPCS puts the first arg in r0), so the
indirection costs nothing measurable relative to calling the vendor
function directly.

The `(port << 5) | pin` shape matches Nordic's own `NRF_GPIO_PIN_MAP`
macro — chosen deliberately so it's a familiar pattern rather than a new
one to learn.

### Bug fix: Silicon Labs adapters weren't enabling their peripheral clocks

Found while integrating `hf_gpio_*` in place of Silicon Labs' own
`sl_simple_led` reference driver (Zlib-licensed, ships as part of the
Gecko SDK/Simplicity SDK "Simple LED" component) as a real test case.
That driver's `sl_simple_led_init()` calls `CMU_ClockEnable(cmuClock_GPIO,
true)` (plus `cmuClock_HFPER` on pre-Series2 parts) before its first
`GPIO_PinModeSet()` — our `hal_port_silabs.c` never did, so `hf_gpio_init()`
silently configured a peripheral with its bus clock gated off; `.text`
size, not functional test coverage, is why the fake-header unit tests
never caught this (the fake `GPIO_PinModeSet()` is a no-op regardless of
real clock state).

STM32's GPIO adapter already does the equivalent
(`hf_enable_port_clock()`, `__HAL_RCC_GPIOx_CLK_ENABLE()`) — that's
precedent this Silicon Labs adapter should have followed from the start.
The asymmetry between vendors here is real, though: STM32's other
peripheral adapters (UART/I2C/SPI/ADC/Timer) get away without their own
explicit clock-enable call because a CubeMX-configured peripheral's
autogenerated MSP init always bundles `__HAL_RCC_*_CLK_ENABLE()` in with
the handle our adapters take by `extern` — there's no way to get an
`huart2`/`hi2c1`/etc. handle without CubeMX having already generated
that. Silicon Labs' classic emlib has no equivalent auto-generated
bundling: a project using `em_gpio.h`/`em_i2c.h`/etc. directly (not going
through an SDK "driver" component) gets nothing for free, which is
exactly why `sl_simple_led` does its own clock enable rather than relying
on board init to have done it.

Once GPIO was fixed, a spot-check of `hal_port_silabs_i2c.c` confirmed
the same root cause there too, so every Silicon Labs adapter was audited
against the actual SDK source (SiliconLabs/simplicity_sdk, sisdk-2025.6)
rather than guessed at. Two genuinely different situations turned up,
not one uniform fix:

**Plain emlib-shaped calls have no clock-enable of their own — fixed in
all of them.** `GPIO_PinModeSet()`, `USART_InitAsync()`, `I2C_Init()`,
`ADC_Init()`, `TIMER_Init()` (classic, `TARGET_SILABS`), and `IADC_init()`
(current SDK's ADC replacement, `TARGET_SILABS_SLGPIO` — genuinely
different hardware from classic ADC, but still a plain emlib-shaped call
with no driver layer above it) all share this gap. Each now gets its own
`CMU_ClockEnable(cmuClock_<peripheral>, true)` unconditionally inside its
`hf_*_init()`, decoded per-instance the same way GPIO's fix works (e.g.
`if (usart == USART0) CMU_ClockEnable(cmuClock_USART0, true);`) — all
idempotent, no "already enabled" guard needed. This is the same fix
repeated per file: `hal_port_silabs_uart.c`, `hal_port_silabs_slgpio_uart.c`
(identical fix — same classic USART hardware either target macro),
`hal_port_silabs_i2c.c`, `hal_port_silabs_adc.c`, `hal_port_silabs_timer.c`,
and `hal_port_silabs_slgpio_adc.c` (IADC).

**"Peripheral HAL"-era driver layers already enable their own clock —
verified clean, no fix needed.** `SPIDRV_Init()` (used unchanged by both
`hal_port_silabs_spi.c` and `hal_port_silabs_slgpio_spi.c`) and
`sl_i2c_init()` (`hal_port_silabs_slgpio_i2c.c`) both call
`sl_clock_manager_enable_bus_clock()` internally — confirmed by reading
`platform/emdrv/spidrv/src/spidrv.c` and `platform/driver/i2c/src/sl_i2c.c`
directly, not assumed from the `sl_gpio_init()` precedent. This is a real,
useful distinction: `sl_gpio_set_pin_mode()` deliberately does *not*
enable its own clock (that's `sl_gpio_init()`'s separate job, since GPIO
has many independent per-pin calls but only one global clock to enable
once), while `sl_i2c_init()`/`SPIDRV_Init()` are each a single "set this
whole peripheral up" call with nothing to split the clock-enable out of —
so they just do it themselves. Silicon Labs' own current-SDK driver
family isn't internally consistent about where clock-enable lives; this
project's fix follows the actual source per peripheral, not a blanket
assumption either direction.

**`sl_sleeptimer` (current SDK's Timer, `hal_port_silabs_slgpio_timer.c`)
is a third shape**: not a per-peripheral bus clock at all, but a global
`sl_sleeptimer_init()` that must run before ANY of its functions work
(including the tick-conversion call `hf_timer_init()` already used) —
confirmed against `platform/service/sleeptimer/src/sl_sleeptimer.c`,
including that it's internally guarded and safe to call every time.
`hf_timer_init()` now calls it unconditionally first, same "call it every
time, let the vendor's own guard handle idempotency" pattern as
`sl_gpio_init()`.

All ten test binaries in `silicon_labs/tests/` still pass (two new fake
files needed: `em_cmu.h`'s enum extended with the USART/I2C/ADC/IADC/
TIMER clock symbols, `sl_gpio_init()`'s fake body added — `sl_sleeptimer`
and classic `TIMER` have no test harness yet, so those two fixes were
verified only by brace-balance and standalone-header-compile checks, the
same lower bar the rest of Timer was held to when it was first written).
The footprint report was regenerated against the real
`arm-none-eabi-gcc` cross-compiler; every changed peripheral grew a
small, expected amount (see `silicon_labs/footprint/FOOTPRINT.md`), and —
usefully corroborating the "no fix needed" finding above — SPI and the
current SDK's I2C `.text` sizes are unchanged, since those two files
weren't touched at all.

## Public API surface (v1, UART — Stage 4)

```c
typedef uint8_t hf_uart_t;   // 0, 1, 2, ... — the vendor's Nth UART/USART

bool hf_uart_init(hf_uart_t instance, uint32_t baud_rate);
void hf_uart_write(hf_uart_t instance, const uint8_t *data, uint32_t len);
uint32_t hf_uart_read(hf_uart_t instance, uint8_t *data, uint32_t len);
```

Same opaque-scalar-plus-lookup-table shape as GPIO's port, chosen for
the same reason (no per-instance object, adapter decodes via a small
`const` array) — but unlike GPIO's port+pin, there's no cross-vendor
numbering scheme to borrow for familiarity: STM32's own USART1/2/3 +
UART4/5 + USART6 naming isn't even contiguous on one vendor, so the
scalar here is just a plain 0-based index, nothing more.

Three design calls worth recording, since each one trades away
something a fuller UART API would have:

- **Fixed 8N1, no flow control, not configurable.** Covers the
  overwhelming majority of debug/echo use cases this project actually
  targets. A parity/stop-bits/flow-control knob is real scope, not
  free — deferred to a later version rather than guessed at now (see
  backlog in `docs/development-plan.md`).
- **No timeout parameter, on either direction.** `hf_uart_write` blocks
  until all bytes are sent; `hf_uart_read` never blocks and returns
  however many bytes (possibly zero) are already available. A
  wall-clock timeout would need some notion of elapsed time, which
  means a dependency on `os_port.h`'s timing primitives — and
  `os_port.h` hasn't been designed yet either (it's later in this same
  Stage 4). Sequencing UART's freeze ahead of a timing dependency it
  doesn't strictly need avoids that ordering problem entirely. This is
  safe specifically *because* there's no flow control in v1: without
  hardware CTS/RTS, TX has nothing external to stall on indefinitely,
  so "block until done" bounds itself in practice.
- **Pin muxing is explicitly out of scope.** `hf_uart_t` identifies a
  peripheral instance, not physical pins — TX/RX alternate-function
  muxing for that instance must already be configured by the
  customer's own vendor tool (CubeMX/SysConfig/pin tool) before
  `hf_uart_init()` runs. This is the same division of responsibility
  GPIO already has on TI MSPM0's IOMUX (see the pull/open-drain
  asymmetry table below) — extended here to a second peripheral
  instead of treated as one-off.

Return-value convention deliberately mirrors GPIO's, not a new scheme:
`hf_uart_init` returns `bool` like `hf_gpio_init` (config/instance may
be invalid); `hf_uart_write` returns `void` like `hf_gpio_write`
(silently does nothing on an invalid instance, since the data argument
itself can't be "wrong"); `hf_uart_read` returns the actual queried
value (byte count) like `hf_gpio_read` returns the actual queried
value (pin state), safe-defaulting to 0/`false` on an invalid handle
either way.

## UART, part two: interrupt/DMA-driven tier

The blocking `hf_uart_write`/`hf_uart_read` pair above was the whole
UART design for about one review cycle, until a real gap got called
out: those two calls only really support a polling-style
implementation, and a wrapper whose answer to "I need interrupt or DMA
mode" is "go use the raw vendor SDK for that" is a wrapper people stop
using — the whole point is that business logic is written once against
`hal_wrapper.h`, not against whichever escape hatch a given feature
gap forces you back to the vendor SDK for. So this tier exists to
close that gap, additively — nothing above changed:

```c
typedef void (*hf_uart_tx_callback_t)(hf_uart_t instance);
typedef void (*hf_uart_rx_callback_t)(hf_uart_t instance);

void hf_uart_set_tx_callback(hf_uart_t instance, hf_uart_tx_callback_t callback);
void hf_uart_set_rx_callback(hf_uart_t instance, hf_uart_rx_callback_t callback);

bool hf_uart_write_async(hf_uart_t instance, const uint8_t *data, uint32_t len);
```

Two different real gaps, so two different fixes:

- **TX had no non-blocking path at all.** `hf_uart_write` is fully
  synchronous by contract — there was no way to say "start sending and
  give me back control now." `hf_uart_write_async` plus
  `hf_uart_set_tx_callback` closes that: kick off a transfer, get
  notified from interrupt context once it's actually left the
  peripheral. Whether a given adapter implements this via a per-byte
  TX interrupt or a DMA channel is a Stage 5 implementation choice,
  documented per vendor when that adapter is written — same treatment
  as the GPIO toggle and pull/open-drain asymmetry tables: real
  differences get written down, not hidden behind one API that
  quietly means different things per vendor.
- **RX already needed a background interrupt/DMA path — it just wasn't
  exposed.** `hf_uart_read`'s own doc comment already required a
  background receive mechanism (real hardware FIFOs are a few bytes
  deep; a polling-only implementation would drop data), so the
  interrupt/DMA machinery `hf_uart_write_async` needed on the TX side
  already had to exist on the RX side to make `hf_uart_read` itself
  correct. `hf_uart_set_rx_callback` just exposes a hook into it — "new
  data landed, go call `hf_uart_read()`" — rather than making callers
  poll blindly for something that was always going to be interrupt-fed
  underneath.

Deliberately **not** exposed: which DMA channel/stream to use, or a
customer-selectable choice between interrupt-mode and DMA-mode for the
same transfer. Both would mean surfacing vendor-specific resource
configuration (DMA channel assignment is deeply part-specific) at the
public API level, which breaks the "thin contract" premise the same
way exposing pin-alternate-function selection would have. A customer
who needs that level of control uses the vendor SDK directly for that
one transfer, same as they would for anything genuinely outside this
wrapper's stated scope — the wrapper's job is to make that the
exception, not the rule.

## UART, part three: all five vendor adapters

Every adapter needs a background receive path regardless of whether
the async tier is ever used — `hf_uart_read()`'s own contract requires
it, since real UART hardware FIFOs are only a few bytes deep. All five
adapters implement the identical pattern to satisfy that: a small
fixed-size ring buffer (`HF_UART_RX_BUFFER_SIZE`, 128 bytes, editable
per file) that a per-vendor interrupt handler continuously fills, and
which `hf_uart_read()` only ever drains. This is the one piece of
UART's implementation that's genuinely uniform across vendors — the
differences below are everywhere else.

- **STM32 HAL**: blocking `hf_uart_write()` uses `HAL_UART_Transmit()`
  directly — a fully separate, synchronous HAL entry point that never
  touches `HAL_UART_TxCpltCallback`, so it can't accidentally trigger a
  registered async callback. RX uses the classic re-arming pattern:
  a single-byte `HAL_UART_Receive_IT()`, re-armed from inside
  `HAL_UART_RxCpltCallback()` on every byte. Same handle-ownership
  model as the STM32 I2C adapter (references CubeMX's own
  `huart1`/`huart2`/`huart3`, trim if not all are enabled) and the same
  "baud rate can genuinely be recomputed at runtime, unlike I2C's
  TIMINGR" reasoning UART's own Stage 4 design already established.
  One STM32-specific gotcha worth recording: because the background
  RX_IT arm never stops, `HAL_UART_GetState()` never settles at
  `HAL_UART_STATE_READY` the way it does between discrete I2C
  transfers — this adapter tracks "is a TX transfer in flight" with
  its own flag instead of relying on HAL's combined state enum.
- **nRF52 (nrfx UARTE)**: verified `nrfx_uarte.h` directly before
  writing this — continuous reception here is buffer-chained, not a
  simple byte interrupt: `nrfx_uarte_rx_enable()` starts continuous
  RX, but the driver asks for the *next* buffer ahead of time via
  `NRFX_UARTE_EVT_RX_BUF_REQUEST` and only hands back completed data
  via `NRFX_UARTE_EVT_RX_DONE`. This adapter ping-pongs between two
  1-byte buffers to get the same per-byte granularity the other
  adapters use directly — simple and correct, at the cost of one
  interrupt per byte rather than per larger chunk (a reasonable future
  optimization, not a correctness requirement). Every real UARTE
  operation goes through the registered event handler (no NULL-handler
  blocking mode is usable here while the background RX path is
  running), so `hf_uart_write()` itself is "kick off `nrfx_uarte_tx()`,
  spin-wait on this file's own busy flag" — same pattern as this
  vendor's `hf_i2c_write_read`. Because that flag is shared between the
  blocking and async TX paths, an `is_async` flag distinguishes them so
  a plain blocking write never spuriously fires the registered TX
  callback.
- **Silicon Labs, classic (emlib)**: `USART_Tx`/`USART_Rx` are already
  blocking single-byte primitives, so `hf_uart_write()` needs no
  interrupts at all here — just a loop. The async tier needs two
  interrupt flags in sequence: `USART_IEN_TXBL` (buffer has room —
  feeds bytes one at a time) hands off to `USART_IEN_TXC` (transmission
  actually complete) once the last byte reaches the shift register;
  reporting completion off `TXBL` alone would fire one byte-time early.
- **Silicon Labs, current (`sl_gpio`/`sl_i2c` target)**: a genuine
  finding, not a shortcut — unlike GPIO and I2C, Silicon Labs has no
  independent "current" UART driver with its own register-level API.
  The nearest thing, `platform/service/iostream`'s `sl_iostream_usart`,
  is a thin wrapper confirmed (via direct source read against
  `SiliconLabs/simplicity_sdk` `sisdk-2025.6`) to take a raw
  `USART_InitAsync_TypeDef*` and raw `USART_TypeDef*` underneath — the
  exact same classic emlib peripheral. Its generic `sl_iostream_t`
  vtable exposes no confirmed-stable public primitive for a
  non-blocking read or TX-completion callback, and mixing its
  internally-managed interrupt handling (implied by its documented
  "receive during sleep" feature) with this file's own hand-rolled RX
  interrupt would risk two pieces of code fighting over the same
  vector and register — a real correctness risk, not a style
  preference. So this adapter bypasses `sl_iostream_usart` entirely and
  talks to the same classic emlib peripheral directly: functionally
  identical to the classic adapter, gated by `TARGET_SILABS_SLGPIO`
  purely for consistency with the vendor's other current-driver
  adapters.
- **TI MSPM0 (driverlib)**: same verification gap as the I2C adapter —
  TI's docs domain and community forum are both blocked in this
  environment. `DL_UART_Main_transmitData`/`receiveData` are confirmed
  (web search against official doc page titles and a working
  community example's described usage), but four more specific names
  (the TX-FIFO-full status flag, and the RX/EOT-done interrupt-index
  and enable-interrupt constants) rely on driverlib's established
  naming pattern rather than a confirmed source read — flagged
  explicitly in the file. A second finding worth recording: one
  community example used a *different*, non-`_Main_`-suffixed function
  family (`DL_UART_receiveData`, `DL_UART_isRXFIFOEmpty`) on a
  different MSPM0 part — some parts have both a full "UART Main" and a
  simpler "UART Extend" peripheral variant with separate driverlib
  APIs, so a customer on the Extend variant needs to drop the
  `_Main_` from every call in this file. `baud_rate` is advisory only,
  same category of limitation as this vendor's I2C `clock_hz` (assumes
  SysConfig's generated init already configured the correct divisor).
  `hf_uart_write_async()` fills the entire TX FIFO up front and waits
  for end-of-transmission — only correct as long as `len` fits within
  the hardware FIFO depth, same scoping caveat as this vendor's
  `hf_i2c_read_async()`.

A design gap that applies to every vendor equally, not just one:
`hf_uart_tx_callback_t` carries no success/failure signal (unlike
I2C's, where a NACK is routine, expected operation) — UART error
reporting was scoped out of v1 entirely. Every adapter above handles a
transfer error the same way: clear the busy flag so a failed transfer
doesn't permanently block future `hf_uart_write_async()` calls, but
never invoke the callback on error, since firing it would incorrectly
imply the transfer succeeded — silence is more honest than a lie here.

## Public API surface (v1, I2C — Stage 4), plus all five vendor adapters

```c
typedef uint8_t hf_i2c_t;   // 0, 1, 2, ... — the vendor's Nth I2C/TWI(M)

bool hf_i2c_init(hf_i2c_t instance, uint32_t clock_hz);
bool hf_i2c_write(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len);
bool hf_i2c_read(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len);
bool hf_i2c_write_read(hf_i2c_t instance, uint8_t device_addr,
                        const uint8_t *tx_data, uint32_t tx_len,
                        uint8_t *rx_data, uint32_t rx_len);

typedef void (*hf_i2c_tx_callback_t)(hf_i2c_t instance, bool success);
typedef void (*hf_i2c_rx_callback_t)(hf_i2c_t instance, bool success);
void hf_i2c_set_tx_callback(hf_i2c_t instance, hf_i2c_tx_callback_t callback);
void hf_i2c_set_rx_callback(hf_i2c_t instance, hf_i2c_rx_callback_t callback);
bool hf_i2c_write_async(hf_i2c_t instance, uint8_t device_addr, const uint8_t *data, uint32_t len);
bool hf_i2c_read_async(hf_i2c_t instance, uint8_t device_addr, uint8_t *data, uint32_t len);
```

Same opaque-instance-plus-lookup-table shape as UART, same async/
callback shape too — deliberately not a third pattern. Two real
divergences from UART's convention, both explained in
`include/hal_wrapper.h`'s own comments: `hf_i2c_write`/`read` return
`bool` (transactional success/fail) rather than a byte count, since
I2C has no meaningful "partial success" the way a stream does; and
the async callbacks carry a `success` flag, since a NACK is routine
I2C operation (bus scanning relies on it), not a rare fault the way a
UART framing error is.

`hf_i2c_write_read` writes `tx_len` bytes then reads `rx_len` bytes
from the same device with a **repeated START** between the two phases
— not a full STOP+START — matching what real I2C sensors and EEPROMs
expect for a "write register address, read its value" transaction.
This one call is where the five vendor adapters diverge the most, so
recording how each achieves it (or doesn't, cleanly) matters more here
than a shared abstraction would suggest:

- **nRF52 (nrfx TWIM)** and **Silicon Labs, both generations** (emlib's
  `I2C_FLAG_WRITE_READ` descriptor flag; the current `sl_i2c` driver's
  own `sl_i2c_transfer()`) all have a **native combined write-then-read
  primitive** — repeated START is just what the vendor's own single
  call already does. No adapter-side sequencing needed.
- **STM32 HAL** has no such single call — this adapter uses the
  Sequential transfer API (`HAL_I2C_Master_Seq_Transmit_IT`/
  `Seq_Receive_IT` with `I2C_FIRST_FRAME`/`I2C_LAST_FRAME`) to hold the
  bus across both phases, spin-waiting on `HAL_I2C_GetState()` between
  them since blocking is a wrapper over an inherently interrupt-driven
  primitive here.
- **TI MSPM0 driverlib** similarly has no single call — this adapter
  uses `DL_I2C_startControllerTransferAdvanced()`'s explicit
  START/STOP control (withhold STOP on the TX phase, request START
  again on the RX phase) to get the same repeated-START behavior by
  hand.

Two further per-vendor asymmetries worth naming rather than leaving
buried in adapter comments:

- **STM32's `clock_hz` is advisory only.** STM32's newer I2C
  peripheral (F4/F7/G0/G4/L4/H7+) configures speed through a single
  `TIMINGR` register whose correct value depends on the clock tree and
  rise/fall-time assumptions — it is not a runtime Hz-to-register
  formula, it's what CubeMX's own timing-calculation tool computes.
  Rather than guess an approximation that could silently violate I2C
  bus timing on real hardware, `hf_i2c_init()` on STM32 assumes
  CubeMX's generated `MX_I2Cx_Init()` already configured the correct
  timing before `main()` runs, and `clock_hz` is accepted but unused.
  Same category of limitation as TI's GPIO pull-resistor gap — a real
  vendor constraint, not an oversight.
- **nRF52 and the current Silicon Labs `sl_i2c` driver need SCL/SDA
  pins supplied directly at init**, unlike STM32/TI where a config tool
  (CubeMX/SysConfig) pre-mux pins invisibly before this wrapper ever
  runs. Both peripherals wire pins via their own init call (PSEL
  registers on nRF52, an `sl_gpio_t` field in `sl_i2c_init_params_t` on
  Silicon Labs) — there's no equivalent pre-muxing step to lean on. Both
  adapters carry an explicit, clearly-marked "EDIT ME" pin table for
  this reason; it's a permanent difference in where pin configuration
  lives, not a placeholder to finish later.

Verification note: STM32, nRF52, and both Silicon Labs I2C adapters
were checked against the vendor's own real header source (or, for
Silicon Labs' newer driver, the actual `SiliconLabs/simplicity_sdk`
source on GitHub) before being written, same discipline as every GPIO
adapter. TI's I2C adapter is the one exception — TI's documentation
domain and community forum are both blocked by this environment's
network egress proxy, so two specific constant names (the RX-FIFO-
ready status flag, and the exact `DL_I2C_IIDX_CONTROLLER_*` interrupt
constants) rely on driverlib's otherwise well-established, consistent
naming pattern rather than a confirmed source read — flagged explicitly
in `ti/i2c/hal_port_ti_mspm0_i2c.c` rather than presented with false
confidence. None of the five I2C adapters have been compiled against a
real SDK or run on real hardware — same standing gap as every adapter
in this repo.

## Public API surface (v1, SPI — Stage 4)

SPI was originally deferred out of month-1 scope entirely (see the
decisions log below — grouped with ADC as carrying the most vendor-
specific quirks, DMA-driven SPI especially) and is pulled forward here
at the user's explicit request, same kind of re-scoping this project
has already done for GPIO (ahead of UART/I2C/PWM in Stage 1) and for
Silabs/TI's GPIO and I2C adapters (ahead of their originally-planned
stages).

```c
typedef uint8_t hf_spi_t;   // 0, 1, 2, ... — the vendor's Nth SPI peripheral

typedef enum {
    HF_SPI_MODE_0, HF_SPI_MODE_1, HF_SPI_MODE_2, HF_SPI_MODE_3
} hf_spi_mode_t;

bool hf_spi_init(hf_spi_t instance, uint32_t clock_hz, hf_spi_mode_t mode);
bool hf_spi_write(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len);
bool hf_spi_read(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len);
bool hf_spi_transfer(hf_spi_t instance, hf_gpio_t cs_pin,
                      const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

typedef void (*hf_spi_tx_callback_t)(hf_spi_t instance);
typedef void (*hf_spi_rx_callback_t)(hf_spi_t instance);
void hf_spi_set_tx_callback(hf_spi_t instance, hf_spi_tx_callback_t callback);
void hf_spi_set_rx_callback(hf_spi_t instance, hf_spi_rx_callback_t callback);
bool hf_spi_write_async(hf_spi_t instance, hf_gpio_t cs_pin, const uint8_t *data, uint32_t len);
bool hf_spi_read_async(hf_spi_t instance, hf_gpio_t cs_pin, uint8_t *data, uint32_t len);
```

Four design decisions worth recording, each a deliberate departure
from (or deliberate reuse of) UART/I2C's precedent:

- **Chip-select composes the existing GPIO API rather than inventing a
  new concept.** SPI has no addressing scheme the way I2C does — the
  bus doesn't know which device is being talked to, a separate CS line
  per device does that job. Every SPI call here takes `cs_pin`, a
  plain `hf_gpio_t` the caller configures with the already-frozen
  `hf_gpio_init(cs_pin, HF_GPIO_OUTPUT_PUSHPULL)`. The adapter asserts
  it (drives low) before the transfer and deasserts it (drives high)
  after — active-low, fixed, not configurable in v1 (the overwhelming
  SPI convention; active-high is backlog). This is the first place two
  of this wrapper's own peripheral APIs compose directly, made
  possible specifically because `hf_gpio_write()` is a plain free
  function with no hidden context to thread through.
- **Full-duplex is the real primitive; write/read are the degenerate
  cases, not the other way around.** Unlike I2C's `write_read` (two
  genuinely sequential phases with a repeated START between them),
  SPI's `tx_data`/`rx_data` in `hf_spi_transfer` are clocked
  *simultaneously* — every bit shifted out on MOSI has a bit shifted
  in on MISO at the same clock edge, because that's what SPI hardware
  physically does. `hf_spi_write`/`hf_spi_read` aren't a separate mode;
  they're `hf_spi_transfer` with the unused side ignored (dummy TX
  bytes for a read, discarded RX bytes for a write). Offered as named
  functions anyway, mirroring I2C's write/read/combined three-function
  shape, since "I only care about one direction" is still the common
  case worth naming directly.
- **`bool` return, not a byte count — for a different reason than
  I2C's.** I2C's blocking calls return `bool` because a NACK is a real
  bus-level failure mode partway through a transfer. SPI doesn't even
  have that: there's no protocol-level acknowledgment at all, so a
  valid instance/pin combination always fully completes a transfer of
  the requested length. `false` here only ever means an invalid
  `instance` or `cs_pin` — closer to `hf_gpio_read`'s safe-default
  convention than to I2C's actual bus-failure reporting, even though
  the return type looks the same.
- **Async callbacks are parameterless, like UART's, not success-
  carrying like I2C's.** Same reasoning as UART's callback shape: no
  NACK-equivalent exists at the SPI protocol level for a callback to
  meaningfully report, so there's nothing for a success flag to add
  beyond what the synchronous `false` return already rejects at the
  call site.

Deferred, same reasoning as `hf_i2c_write_read`'s async omission:
`hf_spi_transfer`'s async equivalent isn't in v1. The blocking version
already covers full-duplex use, and a genuinely simultaneous
completion doesn't fit the separate tx/rx-callback shape any more
cleanly than I2C's two-phase transfer did.

## SPI, part two: all five vendor adapters

The chip-select-via-GPIO design above works exactly as intended in
practice: every adapter below calls `hf_gpio_write(cs_pin, false)`
before the vendor transfer call and `hf_gpio_write(cs_pin, true)`
after, with zero vendor-specific CS handling needed — the first real
confirmation that this wrapper's own peripheral APIs compose cleanly.

- **STM32 HAL**: `HAL_SPI_Transmit`/`Receive`/`TransmitReceive` cover
  the blocking tier directly; the async tier uses the `_IT` variants
  plus the weak `HAL_SPI_TxCpltCallback`/`RxCpltCallback` (overridden
  here) and `HAL_SPI_ErrorCallback`. Same handle-ownership model as the
  I2C/UART adapters — references CubeMX's own `hspi1`/`hspi2`/`hspi3`,
  trim if not all three are enabled. Two settable fields split
  differently than I2C's single advisory-only `clock_hz`: CPOL/CPHA
  (`mode`) has no per-part ambiguity and is applied directly via
  `hspi->Init.CLKPolarity`/`CLKPhase` + `HAL_SPI_Init()`, while
  `clock_hz` stays advisory-only, same category of limitation as I2C's
  TIMINGR — `BaudRatePrescaler` is a simple divisor in principle, but
  computing it correctly needs knowing which clock domain (PCLK1 vs
  PCLK2) feeds this specific SPI instance, which varies by part.
  Requires CubeMX's NSS management set to Software/disabled, since this
  wrapper drives CS itself — a hardware-driven NSS output would fight
  the `cs_pin` argument.
- **nRF52 (nrfx_spim)**: verified `nrfx_spim.h` directly, and it maps
  onto this API unusually cleanly — `NRFX_SPIM_XFER_TRX`/`TX`/`RX`
  descriptor macros are a native full-duplex-or-degenerate transfer
  type, exactly matching `hf_spi_transfer`/`write`/`read`'s own shape,
  so no vendor-specific reshaping was needed the way STM32/TI's
  adapters require. `ss_pin` is left `NRFX_SPIM_PIN_NOT_USED` since CS
  is managed via `cs_pin` on every call, not nrfx's own fixed CS pin.
  Same "every real operation goes through the registered event handler,
  so blocking is kick-off-plus-spin-wait" pattern as this vendor's I2C
  adapter, with the same `is_async` flag to keep a plain blocking call
  from mis-firing the registered async callback.
- **Silicon Labs, both targets**: a genuine finding, the opposite
  conclusion from UART's — there is no dedicated `sl_spi` driver under
  `platform/driver` the way `sl_gpio`/`sl_i2c` exist, but
  `platform/emdrv/spidrv` (SPIDRV, verified directly against
  `SiliconLabs/simplicity_sdk` `sisdk-2025.6`) genuinely supports
  everything needed: real blocking (`SPIDRV_MTransmitB`/`MReceiveB`/
  `MTransferB`) *and* real async with a proper completion callback
  (`SPIDRV_MTransmit`/`MReceive` + `SPIDRV_Callback_t`, which reports
  `Ecode_t transferStatus`, unlike UART's parameterless completion).
  Its handle struct supports both classic `USART_TypeDef` and the newer
  `EUSART_TypeDef` (Series 3) internally, so there's no second driver to
  split against the way GPIO/I2C split — SPIDRV is used **unchanged**
  for both `TARGET_SILABS` and `TARGET_SILABS_SLGPIO`, gated by target
  purely for consistency with this vendor's other adapters, not because
  the implementation differs. `csControl = spidrvCsControlApplication`
  tells SPIDRV not to drive CS itself, same reasoning as every other
  vendor here. **Real hardware-sharing hazard, newly discovered this
  pass**: classic EFM32/EFR32 SPI (via SPIDRV) and classic UART both run
  on the exact same USART peripheral and IRQ vectors — enabling both
  `hal_port_silabs_spi.c`/`hal_port_silabs_slgpio_spi.c` and this
  vendor's UART adapters for the *same* USART instance produces a
  duplicate-symbol link error. Documented prominently in all four
  affected files (both SPI adapters, cross-referenced from both UART
  adapters) rather than only in one place, with the same "trim the
  table to disjoint instances" resolution already established for
  STM32's I2C handle table.
- **TI MSPM0 (driverlib)**: a real verification improvement over this
  vendor's I2C/UART adapters — `software-dl.ti.com` and the TI forum are
  both blocked in this environment, but the official GitHub mirror
  `TexasInstruments/mspm0-sdk` is not, and `source/ti/driverlib/dl_spi.h`
  was fetched directly from it, giving much higher confidence than the
  web-search-snippet verification the I2C/UART adapters had to rely on.
  Confirmed directly: `DL_SPI_Config`/`DL_SPI_init`,
  `DL_SPI_transmitDataBlocking8`/`receiveDataBlocking8`, the four
  `DL_SPI_FRAME_FORMAT_MOTO3_POL*_PHA*` mode constants, and
  `DL_SPI_CHIP_SELECT_NONE` (disables the peripheral's own automatic CS
  output, since this wrapper drives CS itself). `clock_hz` is advisory
  only, same category of limitation as this vendor's I2C/UART
  clock/baud parameters. One real inference, not confirmed by a worked
  example: `DL_SPI_receiveDataBlocking8()` takes no dummy-TX-byte
  parameter, so a receive-only call transmits a dummy byte first to
  generate the clock edges, consistent with how SPI hardware always
  derives its clock from an active transmit. Two names in the async
  tier — `DL_SPI_getPendingInterrupt`, `DL_SPI_disableInterrupt` —
  follow driverlib's confirmed, consistent naming pattern from other
  peripherals (`DL_I2C_getPendingInterrupt`, `DL_GPIO_disableInterrupt`)
  but weren't in this exact fetch, flagged the same way this vendor's
  other adapters flag their own unconfirmed specifics. No whole-buffer
  transfer primitive exists at this level, so `hf_spi_write`/`read`/
  `transfer` all loop byte-by-byte, draining or dummy-feeding the FIFO
  every iteration to avoid RX FIFO overflow — same shape as this
  vendor's own byte-loop UART/I2C patterns.

The same design gap UART's vendor section records applies here too:
`hf_spi_tx/rx_callback_t` carry no success/failure signal, so every
adapter above handles a transfer error identically — clear the busy
flag (and deassert CS) so a failed transfer doesn't permanently block
future async calls, but never invoke the callback on error, since
firing it would incorrectly imply success. None of the five SPI
adapters have been compiled against a real SDK or run on real
hardware — same standing gap as every adapter in this repo.

## Public API surface (v1, ADC — Stage 4)

ADC was grouped with SPI in the original month-1 deferral (see the
decisions log below — "most vendor-specific quirks live there") and is
pulled forward here at the user's explicit request, the same
re-scoping already applied to GPIO and SPI.

```c
typedef uint8_t hf_adc_t;          // 0, 1, 2, ... — the vendor's Nth ADC peripheral
typedef uint8_t hf_adc_channel_t;  // the vendor's own channel number

bool hf_adc_init(hf_adc_t instance);
bool hf_adc_read(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value);

typedef void (*hf_adc_callback_t)(hf_adc_t instance);
void hf_adc_set_callback(hf_adc_t instance, hf_adc_callback_t callback);
bool hf_adc_read_async(hf_adc_t instance, hf_adc_channel_t channel, uint16_t *value);
```

Four design decisions worth recording:

- **Channel, not pin, is the addressing unit — and it's out of scope
  to remap.** Unlike GPIO's port+bit encoding, an ADC channel-to-pin
  mapping is fixed in silicon on every one of these four vendors, not
  a user-selectable alternate-function mux. `hf_adc_channel_t` is
  therefore just the vendor's own channel number, the same "the
  wrapper doesn't invent an addressing scheme the hardware doesn't
  have" reasoning as `hf_i2c_t`'s `device_addr` and SPI's `cs_pin`
  composition — except here there's genuinely nothing to compose, so
  the channel number is passed through directly.
- **Fixed 12-bit resolution, not a configurable enum.** All four
  vendors support 12-bit (STM32's ADC resets to it and supports more;
  nRF52's SAADC supports up to 14-bit but is commonly run at 12;
  Silicon Labs' classic ADC and current IADC both support 12-bit; TI's
  ADC12 is fixed 12-bit by name), so v1 standardizes on the widest
  common denominator rather than adding a resolution parameter no
  adapter would uniformly honor — same "cover the overwhelming
  majority, configurability is backlog" reasoning as UART's fixed 8N1.
  Samples are a plain `uint16_t`, 0-4095.
- **`bool` return with an output parameter, for SPI's reason, not
  I2C's.** A single ADC conversion always either fully completes or
  the `instance`/`channel` was invalid to begin with — there's no
  bus-level partial-failure mode analogous to I2C's NACK — so `false`
  here only ever means an invalid argument, the same convention
  `hf_spi_write`/`read` already established.
- **One callback, not a tx/rx pair.** ADC is receive-only — there's no
  "write" side to this peripheral (an analog output is a DAC, out of
  scope). `hf_adc_callback_t` is parameterless for the same reason
  `hf_spi_tx/rx_callback_t` are: no bus-failure-style outcome exists
  for a single conversion beyond what the synchronous `false` already
  rejects at the call site.

Deferred to backlog, not v1: multi-channel scan mode and continuous/
free-running conversion. Every other peripheral in this header treats
one call as one discrete operation, and ADC follows that shape too —
"convert this one channel, once" is the primitive; a scanning/
continuous mode can be layered on top later without breaking this
contract.

Verified the header still compiles standalone against an expanded stub
with `gcc -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion`, zero
warnings.

## ADC, part two: all five vendor adapters

Unlike GPIO/I2C/UART/SPI, where four vendors meant four files (UART and
SPI shrinking Silicon Labs to a shared driver), ADC needed **five
files across four vendors**: Silicon Labs splits into two genuinely
different adapters here, the same shape as its GPIO/I2C split, not
UART/SPI's shared one.

- **STM32 HAL**: `HAL_ADC_Start`/`PollForConversion`/`GetValue` for
  blocking, `HAL_ADC_Start_IT` plus the weak `HAL_ADC_ConvCpltCallback`/
  `ErrorCallback` for async. Resolution is advisory only — unlike
  SPI/I2C's advisory `clock_hz`, this adapter doesn't even attempt to
  set it at runtime, since calling `HAL_ADC_Init()` would re-run
  CubeMX's entire peripheral configuration using an incomplete
  `ADC_InitTypeDef`; it assumes CubeMX's own generated init already
  selected 12-bit (the reset default on most parts). The channel
  argument passes straight through as `ADC_CHANNEL_0..18` — confirmed
  these are plain sequential integers, not a bitmask, so no lookup
  table is needed the way SPI's `cs_pin` or I2C's `device_addr`
  suggested might be necessary. Every call reconfigures the single
  regular-sequence channel via `HAL_ADC_ConfigChannel()` first, since
  v1 only ever runs one channel per call.
- **nRF52 (nrfx_saadc)**: a genuine finding, the opposite of this
  vendor's I2C/UART/SPI situation — SAADC has a real native blocking
  mode. `nrfx_saadc_simple_mode_set()` called with a NULL event handler
  makes `nrfx_saadc_mode_trigger()` block until the conversion
  finishes, so unlike SPIM/TWIM/UARTE (none of which have a
  NULL-handler blocking mode, forcing "blocking" to be kick-off-plus-
  spin-wait on a busy flag), this adapter's `hf_adc_read()` needs no
  busy-flag choreography at all — only the async path touches it. Only
  one SAADC peripheral exists on nRF52, so `hf_adc_t` only ever has one
  valid value (0), unlike this vendor's two-instance I2C/UART/SPI
  adapters. Channel-to-pin mapping is a fixed 1:1 relationship
  (channel N is always `NRF_SAADC_INPUT_AINn`), so — unlike this
  vendor's I2C/SPI SCL/SDA/SCK pin tables — no EDIT-ME board-wiring
  table is needed here.
- **Silicon Labs, classic (`ADC`, `TARGET_SILABS`)**: a real wrinkle
  specific to this vendor — classic EFM32/EFR32 ADC exposes two
  different channel-selection struct fields depending on part family:
  `input`/`ADC_SingleInput_TypeDef` (simple `adcSingleInputCh0..Ch7`)
  on non-APORT parts, or `posSel`/`ADC_PosSel_TypeDef` (APORT-crossbar-
  routed) on APORT parts. This adapter uses the simpler non-APORT
  field, with an EDIT-ME note for APORT parts — the same "board/
  part-specific, document it" treatment as this vendor's I2C SCL/SDA
  table.
- **Silicon Labs, current (IADC, `TARGET_SILABS_SLGPIO`)**: unlike
  UART/SPI, where no independent current driver exists and both
  targets share one implementation, ADC's current-generation
  replacement (IADC, Incremental ADC) is genuinely different hardware,
  not just a newer driver over the same registers — many Series 2
  parts have IADC only, no classic ADC at all. This is the same
  two-real-adapters shape as GPIO/I2C's split, confirmed by reading
  `em_iadc.h` directly. `posInput`/`IADC_PosInput_t` selects an
  arbitrary GPIO port+pin (not a fixed dedicated analog pin the way
  nRF52's SAADC is), so this adapter carries its own EDIT-ME channel
  table. This file carries more genuinely-unconfirmed specifics than
  any other adapter in this repo — several struct/macro names
  (`IADC_AllConfigs_t`/`IADC_ALLCONFIGS_DEFAULT`, `IADC_SingleInput_t`/
  `IADC_SINGLEINPUT_DEFAULT`, `IADC_Result_t.data`) follow Silicon
  Labs' own published IADC example-project conventions rather than a
  directly re-quoted struct definition, and the interrupt flag/IRQ
  vector names follow this vendor's well-established naming pattern
  without being independently re-confirmed — all flagged explicitly in
  the file, recommending cross-check against Simplicity SDK's own
  "iadc_single_conversion" example projects.
- **TI MSPM0 (driverlib)**: verified directly against the official
  `TexasInstruments/mspm0-sdk` GitHub mirror, same higher-confidence
  source as this vendor's SPI adapter. Real finding, different in
  shape from this vendor's other three peripherals: ADC12 results are
  indexed by a conversion-memory slot (`DL_ADC12_MEM_IDX`, 0-11), not
  read back by raw channel number — each MEM_IDX is configured, via
  `DL_ADC12_configConversionMem()`, to sample one specific channel.
  SysConfig's usual pattern pre-configures a fixed MEM_IDX-to-channel
  mapping once and leaves it alone; this adapter instead deliberately
  reconfigures MEM_IDX 0 to the requested channel on every call, since
  `hf_adc_channel_t` needs to select an arbitrary channel per call.
  `DL_ADC12_configConversionMem()`'s full 9-parameter signature was
  confirmed, but six of its enum-constant arguments (vref, stime,
  avgen, bcsen, trig, wincomp) were not independently confirmed in this
  fetch pass — this adapter passes conservative disabled/software-
  default-style values for all six, flagged explicitly in the file for
  cross-check against the customer's installed SDK.

The same design gap UART's and SPI's vendor sections record applies
here too: `hf_adc_callback_t` carries no success/failure signal, so
every adapter above handles a conversion error identically — clear the
busy flag without invoking the callback, since firing it would
incorrectly imply success. None of the five ADC adapters have been
compiled against a real SDK or run on real hardware — same standing
gap as every adapter in this repo. Sanity-checked all five new files
for balanced braces/parens, same as every prior adapter batch.
`HF_ENABLE_ADC` flipped to `1` in `hal_wrapper_config.h` now that real
adapters exist.

## Unit tests and flash/RAM footprint measurement (all 25 adapters)

Requested after all 25 adapter files existed: a real unit test per adapter,
plus a real per-adapter flash/RAM footprint number — "like actual companies
have for their libs." Both were built out completely rather than as a
proof-of-concept on one vendor, matching this project's existing "every
peripheral, every vendor" completion bar.

**Segregated per vendor, not one shared top-level folder** — first built as
`tests/` and `footprint/` at the repo root covering all five vendors, then
reorganized after the user pointed out the obvious problem: a customer who
only copies `stm/` into their project (this repo's whole "copy only what
you need" model, see "Why not just use X" above) had no reason to also
receive nRF52/Silicon-Labs/TI test and footprint content. Each vendor folder
is now self-contained — `stm/tests/`, `stm/footprint/`, `nrf/tests/`,
`nrf/footprint/`, `silicon_labs/tests/`, `silicon_labs/footprint/`,
`ti/tests/`, `ti/footprint/` — mirroring the same per-vendor structure the
adapters themselves already use. The only thing that stayed shared is
`unity/` at the repo root (vendored [Unity](https://github.com/ThrowTheSwitch/Unity),
MIT licensed): a shared third-party dependency like `include/hal_wrapper.h`
itself, not test *content*, so it doesn't create the same "unwanted vendor
data" problem.

**Unit tests** (153 tests total across 25 files, one per adapter — 34 STM32,
32 nRF52, 56 Silicon Labs [10 files, both SDK generations], 31 TI): each
adapter `.c` file compiles completely unchanged, with the *host's* own
`gcc`, against a hand-written fake of the real vendor SDK header(s) it
includes (`<vendor>/tests/fakes/`) — same function names/signatures/struct
layouts as the real SDK, confirmed against this project's own research
notes recorded in each adapter's header comment, but not a real vendor
header (none is installed in this environment). This is the standard
technique for host-testing an HAL without installing five vendor SDKs —
see each vendor's own `tests/README.md` for the full design writeup,
including a genuinely vendor-specific complication: STM32 and
classic-Silicon-Labs/TI adapters expose ordinary non-static
callback/IRQHandler symbols a test can call directly to simulate an
interrupt firing, but nRF52's event handlers are `static` inside the
adapter, so `nrf/tests/fakes/`'s fakes instead invoke whichever handler was
registered, either synchronously (the default, so this vendor's "kick off,
then spin on our own busy flag" blocking calls terminate on a host machine)
or on manual command (for tests that need to control exactly when a
callback fires). Writing these tests surfaced one genuine bug, not just
confirmed existing behavior: `nrf/uart/hal_port_nrf52_uart.c` calls
`nrfx_uarte_init()` without the explicit context argument this vendor's
I2C/SPI adapters both pass, so its event handler's
`(hf_uart_t)(uintptr_t)p_context` cast has no reliable way to identify
which UARTE instance fired — flagged in `nrf/tests/fakes/nrfx_uarte.h` and
in `nrf/tests/TEST_REPORT.md` rather than silently patched, since fixing it
needs verifying real nrfx's actual context-delivery mechanism first.

**Footprint** (one `FOOTPRINT.md` per vendor): the one place in this repo
that uses a *real* ARM Cortex-M cross-compiler (`arm-none-eabi-gcc`) rather
than a fake/host stand-in — each adapter `.c` file compiles standalone to a
`.o` at `-Os` (never linked into a full firmware image, since no vendor SDK
is installed here) against the exact same fake headers that vendor's unit
tests use, reused for free since compiling to an object file only needs
declarations, not the fakes' behavioral bodies. Each vendor builds for a
real core that vendor ships — Cortex-M4 for STM32/nRF52/classic Silicon
Labs, Cortex-M33 for current-generation Silicon Labs (Series 2/3),
Cortex-M0+ for TI MSPM0 — not one generic core for everything. Getting
`arm-none-eabi-gcc` here was itself notable: this environment's `apt`
mirror throttled the ~460 MB `libstdc++-arm-none-eabi-newlib` package to
the point of being impractical (still under a quarter done after half an
hour), so the toolchain actually used is xPack's prebuilt GCC 13.3.1
release fetched directly from GitHub (each vendor's `footprint/README.md`
has the exact command) — ~70x faster in this environment. See each
`FOOTPRINT.md`'s own "What these numbers do and don't mean" section before
quoting a number out of context — in particular, no vendor SDK is linked
in, so these are the wrapper adapter's own code cost only, not a full
firmware's.

## Unit tests and flash/RAM footprint measurement for Timer and PWM (all 35 adapters)

Requested once Timer and PWM's vendor adapters existed, closing the same gap
every other peripheral already had — this is the direct continuation of
"Unit tests and flash/RAM footprint measurement (all 25 adapters)" above,
same per-vendor infrastructure extended rather than rebuilt: 10 new adapter
files (Timer × 5 targets, PWM × 5 targets) got the identical treatment —
one Unity test file per adapter (per Silicon Labs target macro, matching
that vendor's already-established split), plus a real `arm-none-eabi-gcc`
`.o` build feeding into the same `FOOTPRINT.md` each vendor already had.
64 new tests across 10 new test files, none of it a proof-of-concept —
this brings the wrapper to 35 total adapter files, all with a passing unit
test and a measured footprint.

**New fakes needed, one genuinely new complication per vendor:**
- **STM32** (`stm/tests/fakes/main.h`): added `TIM_HandleTypeDef` (shared by
  Timer and PWM, matching the real HAL's one-struct-per-timer-peripheral
  shape), `TIM_OC_InitTypeDef`, and the `TIM_CHANNEL_1..4` constants at their
  real, non-sequential numeric values (`0x0/0x4/0x8/0xC`, not `0/1/2/3` —
  the fake would have compiled fine with the wrong values too, since nothing
  here checks against real hardware, so this was worth getting right
  deliberately rather than by luck). `__HAL_TIM_SET_COMPARE()`'s real
  behavior (write a register on the peripheral instance pointed to by
  `htim->Instance`) doesn't exist in this fake's simplified handle, so the
  fake's `TIM_HandleTypeDef` carries its own `CCR[4]` array directly and the
  macro just indexes it — a deliberate simplification of real hardware, not
  an attempt to mirror the register layout.
- **nRF52** (`nrf/tests/fakes/nrfx_timer.h`, `nrfx_pwm.h`): Timer's fake is
  a standard registered-event-handler fake, same shape as this vendor's
  existing UART/I2C/SPI fakes. PWM's fake is genuinely different — it has to
  model EasyDMA sequence playback, not a register write, so
  `fake_nrf52_pwm_last_sequence()` returns the actual `nrf_pwm_sequence_t*`
  the adapter handed to `nrfx_pwm_simple_playback()`/`sequence_update()`,
  letting a test dereference `->values.p_individual->channel_N` directly to
  check duty values the adapter wrote into its own static buffer — no
  adapter-internal state needed to be exposed for this to work. Writing this
  fake surfaced a genuine research error from the design pass, not just a
  fake-plumbing bug: `NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY`, which the
  adapter used, isn't a real nrfx macro — caught immediately as a compile
  error once the fake (correctly) didn't define it, fixed in the adapter by
  switching to a project-defined constant, the same pattern already used
  for `HF_NRF52_TIMER_IRQ_PRIORITY`.
- **TI MSPM0** (`ti/tests/fakes/ti_msp_dl_config.h`): added `GPTIMER_Regs`
  (shared by both `DL_TimerG`/Timer and `DL_TimerA`/PWM — real driverlib
  uses the same register-block type for both timer families) with four
  separate fake storage instances (`TIMG0`, `TIMG1`, `TIMA0`, `TIMA1`), and
  `DL_Timer_setCaptureCompareValue`'s test-control counterpart
  (`fake_ti_get_cc_value()`) keyed by `(timer pointer, CC index)` so a test
  can verify per-channel duty independently.
- **Silicon Labs classic** (`silicon_labs/tests/fakes/silabs/em_timer.h`,
  new file): the most involved fake of this whole pass, because
  `hal_port_silabs_pwm.c`'s pin-routing code pokes
  `GPIO->TIMERROUTE[n].CCxROUTE`/`GPIO->TIMERROUTE_SET[n].ROUTEEN`
  registers directly (see that adapter's own header comment on why — no
  emlib function exists for this). The fake had to invent a `GPIO`
  peripheral-level struct that didn't exist anywhere in this vendor's fakes
  before (only the `GPIO_Port_TypeDef` *enum* existed, for classic
  `GPIO_PinModeSet()`'s port argument) — genuinely new fake surface, not an
  extension of existing GPIO fake state. One real, documented fake
  limitation surfaced while writing it: Silicon Labs' real `_SET` register
  alias has OR semantics in hardware (writing a 1-bit only sets that bit;
  0-bits are no-ops), which is exactly why the adapter's plain
  `= 1U << x` assignment is correct on real silicon — but this fake's
  `ROUTEEN` is an ordinary struct field with ordinary C assignment
  semantics, so it can't reproduce that hardware behavior for a test that
  starts two different channels on the same timer and expects both
  enable-bits to still be set afterward. Documented in `em_timer.h`'s own
  comment rather than worked around; no current test depends on it.
- **Silicon Labs current** (`silicon_labs/tests/fakes/silabs_slgpio/
  sl_sleeptimer.h`, `sl_pwm.h`, both new files): `sl_pwm_instance_t` is
  adapter-private static state (an array inside
  `hal_port_silabs_slgpio_pwm.c`, never exposed), so its fake identifies an
  instance by slot number — the Nth distinct `sl_pwm_init()` call, in
  `hf_pwm_init()`'s call order — rather than by pointer, the same
  slot-based approach `fake_nrf52.h` already uses for `nrfx_pwm`'s
  per-instance state. `sl_sleeptimer`'s fake exposes
  `fake_sl_sleeptimer_is_initialized()` specifically to regression-test the
  earlier `sl_sleeptimer_init()` bug (see the Silicon-Labs-clock-enable
  bug-fix section above) — confirms the adapter actually calls it, not just
  that the adapter compiles against a header that declares it.

**A real test-isolation lesson, not just a passing note:** Silicon Labs'
current PWM adapter keeps its "is this channel initialized" state
(`s_valid[][]`) in a `static` array that `fake_silabs_slgpio_reset()`
cannot reach (it's private to the adapter's own translation unit, entirely
by design — the same reason this fake needs slot numbers instead of
pointers at all). A test written to check "operations fail before init"
using a channel an *earlier* test in the same binary had already
initialized failed for exactly this reason on first run — fixed by using a
channel no other test in the file touches, not by adding a reset hook the
adapter's own contract doesn't provide (there is no `hf_pwm_deinit()`).
Documented directly in that test file's own comment so a future test
addition doesn't reintroduce the same ordering trap.

Full test/footprint counts and per-vendor detail are in each vendor's own
`tests/TEST_REPORT.md` and `footprint/FOOTPRINT.md`.

## Public API surface (v1, Timer — Stage 4)

Timer and PWM were the last two peripherals left undesigned in Stage 4.
Requested in that order — periodic timer first, PWM after — and designed
as two genuinely separate peripherals with independent instance-number
spaces (`hf_timer_t`, and a not-yet-designed `hf_pwm_t`), not a combined
"timer" API with a mode flag, because nRF52 backs them with entirely
different hardware: `nrfx_timer`/`nrfx_rtc` for counting vs. the
EasyDMA-driven `nrfx_pwm` for PWM output, with no shared peripheral
underneath the way STM32/Silicon Labs/TI's single TIM-family block can
serve either role. Whether a given vendor's `hf_timer_t` instance 0 and
(eventual) `hf_pwm_t` instance 0 turn out to share the same physical timer
block, and therefore can't both be enabled independently, is a real,
vendor-specific finding left to document once PWM is designed — same
"document it, don't hide it" treatment as Silicon Labs' UART/SPI
USART-sharing conflict already gets.

A quick per-vendor survey before designing (see the session that asked
"what timer/clock APIs exist" before committing to a shape):

- **STM32**: single clear HAL family — basic timers (TIM6/7) for pure
  counting via `HAL_TIM_Base_Init/Start_IT` + the weak
  `HAL_TIM_PeriodElapsedCallback`, general-purpose/advanced timers add PWM
  via `HAL_TIM_PWM_*`. Also has `HAL_Delay()`/`HAL_GetTick()` for blocking
  delay/uptime, unrelated to the TIM peripheral.
- **nRF52**: split across `nrfx_timer` (general-purpose, higher precision)
  and `nrfx_rtc` (low-power, longer periods) for counting — nRF5 SDK's
  `app_timer` software-timer library (built on `nrfx_rtc`) is what
  applications typically use instead of either directly, a scope question
  for later rather than a blocker now. PWM is the separate `nrfx_pwm`.
- **Silicon Labs**: classic emlib's `TIMER` (general-purpose) and
  `LETIMER` (low-energy, for periodic wake) both exist; current-generation
  SDK's usual answer for a periodic/software timer is the `sl_sleeptimer`
  service, analogous to nRF5's `app_timer` — lower confidence, not yet
  independently confirmed against source. PWM has a real, current
  `sl_pwm` driver.
- **TI MSPM0**: same `TIMA`/`TIMG` peripheral family serves both counting
  and PWM, mode-selected (SysConfig usually picks the mode via its GUI).
  Weakest confidence of the four vendors here — no confirmed exact
  function signatures yet, unlike the other three; real vendor-source
  verification is a research task for the implementation pass, same as
  every adapter in this repo.

**Design, `hf_timer_t`:**

```c
typedef uint8_t hf_timer_t;

typedef enum {
    HF_TIMER_ONE_SHOT,
    HF_TIMER_PERIODIC
} hf_timer_mode_t;

bool hf_timer_init(hf_timer_t instance, uint32_t period_us, hf_timer_mode_t mode);
bool hf_timer_start(hf_timer_t instance);
void hf_timer_stop(hf_timer_t instance);

typedef void (*hf_timer_callback_t)(hf_timer_t instance);
void hf_timer_set_callback(hf_timer_t instance, hf_timer_callback_t callback);
```

Key decisions, each mirroring an existing precedent in this header rather
than inventing a new shape:

- **`period_us`, not milliseconds.** Finer than any of these vendors'
  typical periodic-timer use cases strictly need, but matches the
  resolution PWM will need for its own period parameter — same unit
  convention across both peripherals once PWM exists. Translating it into
  the vendor's actual prescaler/auto-reload register values is the
  adapter's job, not the caller's — same "value passed as a rate, adapter
  computes the register value" precedent as `hf_i2c_t`'s `clock_hz` and
  `hf_uart_t`'s `baud_rate`.
- **`hf_timer_init()` configures but does not start** — `hf_timer_start()`/
  `hf_timer_stop()` are a separate control tier. This mirrors every
  vendor's own peripheral API shape here (STM32's `HAL_TIM_Base_Init` vs.
  a separate `_Start`, Silicon Labs' `TIMER_Init` vs. a separate enable,
  etc.) and lets a caller pause/resume a configured timer without
  recomputing prescaler/reload values on every restart — configuration
  (period, mode) is retained across a stop/start cycle.
- **Both `HF_TIMER_ONE_SHOT` and `HF_TIMER_PERIODIC` in v1** — unlike some
  earlier peripherals here where only one mode was table-stakes and a
  second was deferred to backlog (UART's fixed 8N1, SPI's fixed MSB-first),
  both timer modes are equally simple, commonly needed, and supported by
  all four vendors' underlying hardware, so there's no reason to cut
  either from v1.
- **One parameterless callback, not a tx/rx pair** — same reasoning as
  `hf_adc_callback_t`/`hf_spi_tx/rx_callback_t`: a period elapsing has no
  bus-failure-style outcome for a success flag to report. For
  `HF_TIMER_ONE_SHOT`, the timer stops itself after firing once; calling
  `hf_timer_start()` again re-arms it.
- **No blocking delay function** (e.g. no `hf_timer_delay_ms()`). Unlike
  UART/I2C/SPI's blocking calls, which block on a genuinely bounded
  hardware transfer, a delay's "how do we wait" question is really an
  RTOS/scheduler concern (busy-spin vs. yield-to-scheduler) — the same
  reasoning this header already gives for omitting UART timeout parameters
  pending `os_port.h`'s (not yet designed) timing primitives. A one-shot
  `hf_timer_t` plus the caller's own spin-on-a-flag-set-by-the-callback
  already covers this without a dedicated function; a real "block this
  thread, don't busy-spin" delay belongs in `os_port.h` once that's
  designed, not here.
- **No monotonic uptime/"clock" read** (an Arduino `millis()`-style
  function) in this peripheral — a genuinely different capability from a
  periodic callback timer (a passive read vs. an active callback), so
  it's out of scope for `hf_timer_t` specifically; revisit as its own
  thing if a real need surfaces.
- **No mid-period elapsed/remaining-count read** — backlog, same
  "cover the common case first" reasoning as ADC's deferred multi-channel
  scan mode.

No vendor adapters exist yet — `HF_ENABLE_TIMER` is `0` in
`hal_wrapper_config.h`, and this section is frozen but unimplemented,
same state ADC's design pass left things in before its own implementation
pass. PWM is next.

## Timer, part two: all five vendor adapters

Implemented immediately after the design above, same two-step
design-then-implement shape as SPI and ADC. Researched each vendor's real
timer API via direct GitHub source fetches before writing, same discipline
as every prior implementation pass: STM32Cube HAL
(`stm32f4xx-hal-driver`), nrfx (`NordicSemiconductor/nrfx`, both
`nrfx_timer.h` and the lower-level `hal/nrf_timer.h` for the event/shortcut
enums), Silicon Labs classic emlib (`SiliconLabs/Gecko_SDK`, `gsdk_4.5`)
and current `sl_sleeptimer` (`SiliconLabs/simplicity_sdk`, `sisdk-2025.6`),
and TI's official `TexasInstruments/mspm0-sdk` mirror (`dl_timer.h` and
`dl_timerg.h`).

Structural finding, five files across four vendors, same shape as ADC's
classic-vs-IADC split: Silicon Labs needed two genuinely different
adapters again — not because classic `TIMER` and current `sl_sleeptimer`
are different hardware (they aren't; `sl_sleeptimer` is a software service
that still runs on real timer hardware underneath), but because they are
architecturally different *kinds* of API. `hal_port_silabs_timer.c`
targets one hardware peripheral per `hf_timer_t` instance, same as every
other vendor here; `hal_port_silabs_slgpio_timer.c` targets a software
timer *service* where one shared hardware timer backs an effectively
unlimited number of independent handles — the opposite constraint shape
from, say, nRF52's single-instance SAADC. This adapter still exposes a
small fixed-size `hf_timer_t` table for consistency with every other
vendor's instance-count convention, but documents plainly that the limit
is a software choice here, not a hardware one, unlike everywhere else in
this repo.

Real per-vendor findings, each documented in its own file:

- **STM32**: TIM6/TIM7 ("basic timers" — no I/O pins, no capture/compare
  channels, purely counters) are the right hardware for a headless
  periodic timer, same reasoning as picking basic ADC/USART blocks
  elsewhere in this project over more capable but unnecessary
  alternatives. One-shot is software-enforced (the elapsed callback calls
  `HAL_TIM_Base_Stop_IT()` itself) rather than using STM32's own
  hardware One-Pulse Mode, since OPM is fundamentally an output-channel/
  pulse-generation feature (needs a configured output channel, normally
  driven by an input trigger) — a genuine mismatch for this headless,
  channel-less contract, not a shortcut.
- **nRF52**: uses TIMER1/TIMER2, not TIMER0 — TIMER0 is conventionally
  reserved for the SoftDevice/radio stack on many real nRF52 projects, a
  deliberate choice rather than an arbitrary starting index. Unlike this
  vendor's other adapters, `period_us` here is genuinely accurate, not
  advisory: this project owns the timer's clock configuration outright
  (1 MHz, 32-bit width) with no CubeMX/SysConfig-style external
  pre-configuration step to defer to or guess at, and
  `nrfx_timer_us_to_ticks()` handles the conversion exactly. One-shot vs.
  periodic is a genuine hardware feature too:
  `nrfx_timer_extended_compare()`'s shortcut mask wires the compare event
  directly to a hardware STOP or CLEAR task with no CPU intervention
  between periods. Confirmed `nrfx_timer_config_t` carries its own
  `p_context` field delivered straight to the event handler — this
  adapter does NOT repeat the context-delivery gap flagged in this
  vendor's UART adapter, since the timer driver's API shape genuinely
  supports it correctly.
- **Silicon Labs classic**: `TIMER_Init_TypeDef` has its own `oneShot`
  field — a genuine hardware one-shot feature, same as TI's below, not
  software-enforced the way STM32's basic timer needs. `TIMER_IF_OF`/
  `TIMER_IEN_OF` (overflow interrupt, used as the "period elapsed" event)
  follow emlib's well-established naming convention but weren't
  independently re-confirmed by a direct quote in this fetch pass —
  flagged the same way this vendor's other adapters flag their own
  unconfirmed specifics.
- **Silicon Labs current**: the real finding here is a resolution loss,
  not a structural one — `sl_sleeptimer.h` exposes only millisecond-
  granularity time conversion (`sl_sleeptimer_ms_to_tick`/`ms32_to_tick`),
  no microsecond variant, unlike every other vendor this header targets.
  `period_us` is rounded UP to whole milliseconds on this vendor
  specifically (a period under 1000us still costs at least 1ms) — a
  permanent, documented limitation, not an oversight.
- **TI MSPM0**: `DL_TimerG` (general-purpose) was chosen over `DL_TimerA`
  (MSPM0's advanced/capture-compare-and-PWM-capable family) deliberately,
  reserving TimerA for the not-yet-designed PWM peripheral so the two
  don't compete for the same hardware by default — same "right hardware
  for the right job" reasoning as STM32's basic-vs-general-purpose timer
  choice. `DL_TIMER_TIMER_MODE_ONE_SHOT` is confirmed as a genuine
  hardware feature ("one shot mode in DOWN COUNTING," per the SDK's own
  description) — no software enforcement needed, same as classic Silicon
  Labs' `oneShot` field. Five specific names flagged as NOT independently
  confirmed in this fetch pass, each noted individually in the file:
  `DL_TIMER_CLOCK_BUSCLK`, `DL_Timer_TimerConfig.startTimer`'s
  "don't auto-start" value, `.genIntermInt`'s "disabled" value,
  `DL_TIMER_INTERRUPT_ZERO` (inferred as `DL_TIMER_IIDX_ZERO`'s
  enable-side counterpart, same convention already flagged for
  `DL_SPI_disableInterrupt`/`DL_ADC12_disableInterrupt`), and the
  `TIMG0_IRQHandler`/`TIMG1_IRQHandler` vector names (following this
  vendor's own established per-peripheral pattern, not confirmed
  directly for TimerG in this fetch pass).

The same design gap this header already documents for ADC/SPI/UART
applies here too: `hf_timer_callback_t` carries no success/failure
signal, so there is no "error path" to handle differently across these
five adapters the way UART/SPI/ADC's error callbacks needed the "no false
success" rule — a timer period elapsing has no comparable failure mode.
Sanity-checked all five new files for balanced braces/parens, same as
every prior adapter batch — none have been compiled against a real SDK or
run on real hardware, same standing gap as every adapter in this repo.
`HF_ENABLE_TIMER` flipped to `1` in `hal_wrapper_config.h` now that real
adapters exist.

## Public API surface (v1, PWM — Stage 4)

Researched each vendor's real PWM API before designing anything, same
"code research before design" discipline this project used for Timer:
STM32's `stm32f4xx_hal_tim.h`/`.c` (`HAL_TIM_PWM_*` family),
nRF52's `nrfx_pwm.h`, Silicon Labs classic's `em_timer.h` (the same
`TIMER_InitCC`/compare-channel API this vendor's Timer adapter already
uses `TIMER_Init`/`TIMER_TopSet` from) and the current SDK's
`platform/driver/pwm/inc/sl_pwm.h`, and TI's `dl_timera.h`/`dl_timer.h`
(`DL_Timer_initFourCCPWMMode`, confirmed via `dl_timer.c`'s real
prototypes the same way Timer's TI research needed the `.c` source for
`DL_Timer_setCaptureCompareValue`'s exact signature).

**The universal shape, confirmed on every vendor**: one physical timer
owns a shared frequency (STM32's `TIM_Base.Period`/`Prescaler`, nRF52's
`nrfx_pwm_config_t.top_value`, Silicon Labs classic's `TIMER_TopSet()`,
TI's `DL_Timer_PWMConfig.period`) and N independent output-compare
channels, each with its own duty cycle, live-updatable without
reinitializing the timer (STM32's `__HAL_TIM_SET_COMPARE` macro,
Silicon Labs classic's buffered `TIMER_CompareBufSet()`, TI's
`DL_Timer_setCaptureCompareValue()`, nRF52's `nrfx_pwm_sequence_update()`).
This is a genuinely different hardware shape from every other
peripheral in this header (all of which are flat, single-level
instance numbers) — GPIO's own `(port << 5) | pin` encoding turned out
to be the right precedent to reuse rather than introducing a second
opaque type: `hf_pwm_t` is `(timer_instance << 2) | channel`, decoded
the same "scalar, const lookup table in flash" way as everywhere else
in this header. See `hal_wrapper.h`'s own extensive comment on
`hf_pwm_t` for the full API and the shared-frequency caveat that
follows directly from this hardware shape (initializing a second
channel on the same timer with a different `freq_hz` changes the first
channel's frequency too — a real hardware constraint this API surfaces
rather than adding software to hide).

**Per-vendor findings:**

- **STM32**: basic timers (TIM6/TIM7, which `hf_timer_t` already uses)
  have no output-compare channels or pins at all — PWM is
  unconditionally forced onto a different, general-purpose timer
  (TIM2/TIM3/...) regardless of instance numbering on either side. So
  STM32 is disjoint from `hf_timer_t` by hardware, not by convention.
- **nRF52**: `nrfx_pwm` is a genuinely different, more involved shape
  than every other vendor here — not a simple "write the compare
  register" model but an EasyDMA sequence player. `nrfx_pwm_simple_playback()`
  takes a `nrf_pwm_sequence_t` (a RAM-resident array of duty values)
  plus a `playback_count` and flags; `NRFX_PWM_FLAG_LOOP` is what makes
  a single constant duty cycle repeat indefinitely without CPU
  intervention, and `nrfx_pwm_sequence_update()` changes the duty
  live during playback. The duty-value array must live in RAM the
  whole time PWM is running (EasyDMA reads it continuously), so this
  vendor's adapter needs a persistent per-channel static buffer, not a
  stack-local one — a real implementation wrinkle to flag when adapters
  are written, not a design-level concern. Disjoint from `hf_timer_t`
  by hardware (nrfx_timer vs. nrfx_pwm are entirely separate
  peripheral blocks), confirmed already in `hf_timer_t`'s own header
  comment.
- **Silicon Labs classic (`TARGET_SILABS`)**: the one vendor where
  `hf_timer_t` and `hf_pwm_t` genuinely can collide. Both are built on
  the same emlib `TIMER_TypeDef` family (`TIMER_InitCC()` with
  `mode = timerCCModePWM`, `TIMER_CompareSet()`/`CompareBufSet()` for
  duty), and `hf_timer_t`'s adapter already claims TIMER0/TIMER1. The
  PWM adapter must pick different TIMERn instances if both peripherals
  are enabled in the same project — some smaller EFR32 parts only have
  two TIMER blocks total, in which case they genuinely cannot support
  both `hf_timer_t` and `hf_pwm_t` simultaneously. Flagged in both
  `hal_wrapper.h` and `hal_wrapper_config.h`, same treatment as this
  vendor's UART/SPI USART-sharing conflict.
- **Silicon Labs current (`TARGET_SILABS_SLGPIO`)**: unlike Timer
  (where the current SDK replaced the model entirely with the
  software-service `sl_sleeptimer`), the current SDK's PWM story is a
  third shape again — `platform/driver/pwm/inc/sl_pwm.h` is a real,
  current-generation "Peripheral HAL" driver (`sl_pwm_init/start/stop`,
  `sl_pwm_set_duty_cycle`), but confirmed via its own struct
  (`sl_pwm_instance_t.timer` is literally a `TIMER_TypeDef *`) to sit
  directly on top of the exact same classic emlib TIMER hardware as
  `TARGET_SILABS`'s PWM adapter, not a genuinely different peripheral
  the way `sl_gpio`/`sl_i2c` are over classic GPIO/I2C. This project
  uses `sl_pwm` anyway (it's a real driver Silicon Labs ships and
  documents for this SDK generation, same "use the vendor's current
  driver" choice already made for `sl_gpio`/`sl_i2c`/`sl_sleeptimer`),
  rather than bypassing it for raw `TIMER_InitCC()` calls the way the
  UART/SPI adapters bypass `sl_iostream_usart` — the difference being
  `sl_pwm` actually covers what this project needs, where
  `sl_iostream_usart` didn't. The one real cost: `sl_pwm_set_duty_cycle()`
  takes a plain `uint8_t percent` (0-100), not the millipercent
  resolution `hf_pwm_set_duty()` promises — a genuine, permanent
  resolution loss on this SDK generation specifically, the same
  category of finding as this SDK's `sl_sleeptimer` millisecond
  rounding for Timer. Disjoint from `hf_timer_t` in practice (Timer's
  `sl_sleeptimer` runs on RTCC/BURTC, not classic TIMER).
- **TI MSPM0**: `DL_TimerA` (`DL_Timer_initFourCCPWMMode`,
  `DL_Timer_setCaptureCompareValue(gptimer, value, ccIndex)` — the
  exact signature needed fetching `dl_timer.c`, same as Timer's TI
  research) was deliberately reserved for this back when `hf_timer_t`'s
  adapter chose `DL_TimerG` instead — already disjoint by design, no
  new finding needed here. `DL_Timer_PWMConfig.pwmMode` also exposes
  `DL_TIMER_PWM_MODE_CENTER_ALIGN` alongside the edge-aligned default —
  backlog, not v1.

`hf_pwm_t`'s `duty_millipercent` unit (0-100000) was chosen over a raw
compare-register value (no single number means the same thing across
STM32's up-to-32-bit ARR, nRF52's 15-bit `top_value`, Silicon Labs'
16/24-bit TOP, and TI's 32-bit period) and over a plain 0-100 percent
(too coarse for a smooth fade — 1% steps are visible on an LED, and
this project would otherwise inherit Silicon Labs current SDK's own
coarser resolution as its whole API's ceiling rather than as one
documented vendor asymmetry).

No vendor adapters written yet — this is a design-only pass, same
staging Timer went through (design and freeze first, adapters as a
separate follow-up request). `HF_ENABLE_PWM` stays `0` in
`hal_wrapper_config.h` until adapters exist.

## PWM, part two: all five vendor adapters

Implemented immediately after the design pass, same "design and freeze
first, adapters as a separate follow-up request" staging Timer went
through. All five per-vendor findings from the design pass held up
once written as real code; two new findings surfaced only once the
adapters were actually being coded:

- **STM32** (`stm/pwm/hal_port_stm32_pwm.c`): TIM2/TIM3, general-
  purpose 4-channel timers, disjoint from `hf_timer_t`'s TIM6/TIM7 by
  hardware (basic timers have no CC channels at all). `hf_pwm_t`'s
  channel field decodes to `TIM_CHANNEL_1..4` via a lookup table, not
  arithmetic — confirmed these are `0x0/0x4/0x8/0xC`, not `0/1/2/3`.
  `HAL_TIM_PWM_ConfigChannel`'s exact signature wasn't re-quotable from
  a fetched source (the same large-file-truncation gap hit for other
  STM32 HAL functions elsewhere in this project) — proceeded on this
  family's extremely stable, uniform `HAL_TIM_*_ConfigChannel(htim,
  config, Channel)` convention, flagged rather than presented as
  independently re-verified.
- **nRF52** (`nrf/pwm/hal_port_nrf52_pwm.c`): the most structurally
  different adapter of the five, confirming the design pass's finding
  that `nrfx_pwm` is a sequence-player, not a register-write API. Two
  PWM instances (PWM0/PWM1), each needing a persistent (static, not
  stack) `nrf_pwm_values_individual_t` — its 4 fields are named
  `channel_0`..`channel_3`, not an array, confirmed via `hal/nrf_pwm.h`
  — since EasyDMA reads this buffer continuously while
  `NRFX_PWM_FLAG_LOOP` keeps it playing. `hf_pwm_set_duty()` writes the
  one field for its own channel, then calls
  `nrfx_pwm_sequence_update()` to re-arm playback from the (now
  modified) buffer if already running — nRF52's equivalent of every
  other vendor's "live compare-register write", just shaped
  differently. Needed an EDIT-ME `output_pins` table, same situation as
  this vendor's I2C SCL/SDA table: nrfx_pwm has no separate pin-mux
  step, the pin numbers are required at `nrfx_pwm_init()` time. `freq_hz`
  is genuinely accurate (not advisory), same reasoning as `hf_timer_t`'s
  nRF52 adapter: this project owns the clock config outright (fixed
  `NRF_PWM_CLK_1MHz`), so `top_value = 1000000 / freq_hz` is exact.
  `NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY` doesn't actually exist (the
  design-pass research assumed a vendor default macro that isn't real,
  caught by a follow-up fetch during implementation) — used a
  project-defined constant instead, same "our own constant, not an
  assumed SDK default" precedent `hf_timer_t`'s nRF52 adapter already
  set with `HF_NRF52_TIMER_IRQ_PRIORITY`.
- **Silicon Labs classic** (`silicon_labs/pwm/hal_port_silabs_pwm.c`,
  `TARGET_SILABS`): confirmed as the one vendor where `hf_timer_t` and
  `hf_pwm_t` genuinely share hardware — TIMER2/TIMER3 used here,
  disjoint from Timer's TIMER0/TIMER1, with an explicit "verify your
  part has enough TIMER blocks" flag. `mode = timerCCModePWM` alone
  wasn't the whole story: reading Silicon Labs' own current-SDK
  `sl_pwm.c` source (not just `em_timer.h`'s struct comments) revealed
  `cmoa = timerOutputActionToggle` is paired with it on Series 2 devices
  — a real detail this project would have missed by reading only the
  header, the same "the .c source has ground truth the header doesn't"
  lesson already learned fetching STM32's `HAL_TIM_PeriodElapsedCallback`
  and TI's `DL_Timer_setCaptureCompareValue` for Timer. Pin routing
  turned out to be the genuinely hard part: classic emlib's
  `TIMER_InitCC()` doesn't connect a CC channel to a pin at all, and
  Silicon Labs has at least three real, incompatible schemes across
  device generations for doing so (Series 0/1's `TIMER_TypeDef`-level
  `ROUTEPEN`/`ROUTELOC0`; older Series 2's per-timer
  `GPIO->TIMER0ROUTE[]` arrays; newer Series 2's combined
  `GPIO->TIMERROUTE[]`). Implemented the newer Series 2 scheme only
  (confirmed by reading `sl_pwm.c`'s real register-poking code directly,
  including that the `ROUTEEN` enable bit is set in `hf_pwm_start()`,
  not `hf_pwm_init()` — matching sl_pwm's own split), and flagged the
  other two schemes as EDIT ME rather than guessing at unconfirmed
  register layouts for device generations this fetch pass didn't cover
  — same "flag what's genuinely uncertain" treatment as this vendor's
  IADC adapter.
- **Silicon Labs current** (`silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c`,
  `TARGET_SILABS_SLGPIO`): uses `sl_pwm`, confirmed by reading
  `sl_pwm.c` to internally handle everything the classic adapter has to
  do by hand — its own TIMER and GPIO bus clock enable, the GPIO pin
  mode, and the same `GPIO->TIMERROUTE`/`ROUTEEN` connection the classic
  adapter implements manually. `freq_hz` is genuinely accurate here (a
  real improvement over classic's advisory-only assumption) since
  `sl_pwm_config_t.frequency` takes a plain Hz value the driver itself
  converts correctly. `duty_millipercent` is rounded to the nearest
  whole percent — confirmed permanent resolution loss, `sl_pwm_set_duty_cycle()`
  only takes `uint8_t percent`. Also uses TIMER2/TIMER3, purely for
  consistency with the classic adapter's choice — this target's
  `hf_timer_t` uses `sl_sleeptimer` (RTCC/BURTC), so there's no real
  sharing risk here the way there is on `TARGET_SILABS`.
- **TI MSPM0** (`ti/pwm/hal_port_ti_mspm0_pwm.c`): `DL_TimerA`
  (TIMA0/TIMA1), already reserved when `hf_timer_t`'s adapter chose
  `DL_TimerG` — no new hardware-sharing finding needed, confirming the
  design pass's expectation. Reused Timer's TI adapter's exact
  `DL_Timer_ClockConfig` pattern and its flagged-not-guessed treatment
  of `DL_TIMER_CLOCK_BUSCLK` and `startTimer`'s "off" value, since both
  structs share the same field names and this project has no way to
  independently re-verify them any further than Timer's own research
  already did.

Sanity-checked all five new files for balanced braces/parens/brackets.
Verified `hal_wrapper.h` still compiles standalone for every target
macro after flipping `HF_ENABLE_PWM` to `1` in `hal_wrapper_config.h`.
None of the five have been compiled against a real SDK or run on real
hardware — Silicon Labs classic's PWM adapter (the pin-routing scheme
uncertainty) and nRF52's (the most structurally different of the five)
are the highest-priority to verify first.

## Vendor scope, in priority order

1. **STM32** (HAL) — reference adapter, real hardware on hand (Black Pill).
   One adapter covers all current STM32Cube firmware packages; ST's
   older Standard Peripheral Library was end-of-life'd years ago and
   isn't a live migration path the way Nordic's and Silabs' SDK
   transitions below are, so it's not covered.
2. **nRF52** (nrfx) — second vendor, the actual portability proof: same
   example app, only the target macro changes. One adapter covers both
   the legacy nRF5 SDK and the current nRF Connect SDK, since both
   bundle the same unchanged nrfx `nrf_gpio.h` — see
   `nrf/gpio/hal_port_nrf52.c` for why there's no second nRF target.
3. **Silicon Labs EFR32** (emlib **and** the current `sl_gpio` driver)
   — third vendor, confirms generality; simplest of the port-split
   vendors (port+index, not bitmask). Unlike nRF, Silabs' two SDK
   generations are genuinely different, incompatible peripheral APIs
   (`sl_gpio_t` is a `{port, pin}` struct passed by pointer, not two
   loose args; different function names; `sl_status_t` returns) — not
   just a rename — so this is two real adapters,
   `silicon_labs/gpio/hal_port_silabs.c`
   (emlib, `TARGET_SILABS`, still supported, no Series 3) and
   `silicon_labs/gpio/hal_port_silabs_slgpio.c` (`sl_gpio`, `TARGET_SILABS_SLGPIO`,
   production-quality as of the 2025 SDK releases, Series 2 + Series 3).
   Both implement the identical `hf_gpio_*` contract, so migrating
   between them touches only the target macro, never the customer's
   business logic.
4. **TI MSPM0** (driverlib) — backlog / month 2. Bitmask+port shape close
   to STM32's, but MSPM0 projects are typically built around TI's
   SysConfig tool, which pre-configures pin mode/mux before `main()` —
   our `hf_gpio_init()` on this vendor may be a thin pass-through rather
   than doing real configuration work, and the docs need to say why.

ESP32 and NXP are explicitly deferred/dropped — see decisions log below.

## Toggle: a known, permanent asymmetry — document it, don't hide it

| Vendor | Native hardware toggle? |
|---|---|
| STM32 | Yes — `HAL_GPIO_TogglePin` |
| Silicon Labs | Yes — `GPIO_PinOutToggle` |
| TI MSPM0 | Yes — `DL_GPIO_togglePins`, explicitly no R-M-W |
| nRF52 | No — GPIO peripheral exposes only `OUT`/`OUTSET`/`OUTCLR`, no dedicated toggle register. Confirmed: Nordic's own `nrf_gpio_pin_toggle()` in the nRF5 SDK does a read-modify-write of `OUT` into `OUTSET`/`OUTCLR`, same as the ESP32 case. The adapter calls that SDK function directly rather than reimplementing the RMW. |

`hf_gpio_toggle()` in the public API stays vendor-agnostic, but each
adapter either calls the native op or falls back to read+write — and the
README states plainly which vendors get which, rather than presenting
toggle as uniformly zero-cost.

## Pull resistors / open-drain: a second asymmetry, on TI MSPM0 only

| Vendor | `HF_GPIO_INPUT_PULLUP`/`PULLDOWN`/`OUTPUT_OPENDRAIN` reachable from `hf_gpio_t` alone? |
|---|---|
| STM32 | Yes — `GPIO_InitTypeDef.Pull`/`Mode`, same port+pin the rest of the API uses |
| nRF52 | Yes — `nrf_gpio_cfg()`'s pull/drive args, same pin number |
| Silicon Labs (both) | Yes — `out` argument on the same mode-set call |
| TI MSPM0 | **No.** Pull resistors, open-drain, and drive strength live in the IOMUX `PINCM` register, addressed by a per-pin index with no formulaic relationship to the port+bit encoding `hf_gpio_t` carries — it's an arbitrary per-chip mapping normally baked into SysConfig-generated `ti_msp_dl_config.c`. `hf_gpio_init()` on this vendor can only toggle direction (`DL_GPIO_enableOutput`/`disableOutput`, which *are* reachable from port+bit); it returns `false` for the three modes it can't fulfill rather than silently doing half the job. Configure pull/open-drain for a pin in SysConfig's pinmux GUI instead. |

This is the same "document it, don't hide it" call as the toggle table
above, just for a different vendor and a different capability.

## Repo layout

Adapters live under one folder per vendor, split into `gpio/`/`i2c/`/
`uart/` subfolders per peripheral (not a flat `/src/`) — this is the
current, followed-going-forward layout, not a placeholder:

```
/include/hal_wrapper.h                    public API, vendor-agnostic
/include/hal_wrapper_config.h             user-edited feature toggles
/include/os_port.h                        RTOS-shim contract
/unity/                                   vendored Unity test framework (MIT) - shared dependency, not vendor content
/stm/gpio/hal_port_stm32.c
/stm/i2c/hal_port_stm32_i2c.c
/stm/uart/hal_port_stm32_uart.c
/stm/spi/hal_port_stm32_spi.c
/stm/adc/hal_port_stm32_adc.c
/stm/timer/hal_port_stm32_timer.c
/stm/pwm/hal_port_stm32_pwm.c             TIM2/TIM3 — disjoint from Timer's TIM6/TIM7 by hardware
/stm/tests/                               unit tests + fakes for this vendor only: `make -C stm/tests report`
/stm/footprint/                           flash/RAM measurement for this vendor only: `make -C stm/footprint report`
/nrf/gpio/hal_port_nrf52.c                covers both nRF5 SDK and nRF Connect SDK
/nrf/i2c/hal_port_nrf52_i2c.c             ditto
/nrf/uart/hal_port_nrf52_uart.c           ditto
/nrf/spi/hal_port_nrf52_spi.c             ditto
/nrf/adc/hal_port_nrf52_adc.c             ditto (nrfx_saadc)
/nrf/timer/hal_port_nrf52_timer.c         ditto (nrfx_timer)
/nrf/pwm/hal_port_nrf52_pwm.c             nrfx_pwm — separate hardware, EasyDMA sequence player
/nrf/tests/                               unit tests + fakes for this vendor only
/nrf/footprint/                           flash/RAM measurement for this vendor only
/silicon_labs/gpio/hal_port_silabs.c              classic emlib (TARGET_SILABS)
/silicon_labs/gpio/hal_port_silabs_slgpio.c       current sl_gpio driver (TARGET_SILABS_SLGPIO)
/silicon_labs/i2c/hal_port_silabs_i2c.c           classic emlib
/silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c    current sl_i2c driver
/silicon_labs/uart/hal_port_silabs_uart.c         classic emlib
/silicon_labs/uart/hal_port_silabs_slgpio_uart.c  same emlib peripheral underneath — see UART part three
/silicon_labs/spi/hal_port_silabs_spi.c           SPIDRV — shares USART hw with classic UART, see SPI part two
/silicon_labs/spi/hal_port_silabs_slgpio_spi.c    same SPIDRV driver, unchanged — see SPI part two
/silicon_labs/adc/hal_port_silabs_adc.c           classic emlib ADC
/silicon_labs/adc/hal_port_silabs_slgpio_adc.c    IADC — genuinely different hardware, see ADC part two
/silicon_labs/timer/hal_port_silabs_timer.c       classic emlib TIMER
/silicon_labs/timer/hal_port_silabs_slgpio_timer.c  sl_sleeptimer service — genuinely different API shape, see Timer part two
/silicon_labs/pwm/hal_port_silabs_pwm.c           classic emlib TIMER2/3 CC channels — shares hardware family with Timer, see PWM part two
/silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c    sl_pwm — thin wrapper over the same classic TIMER hardware, see PWM part two
/silicon_labs/tests/                             unit tests + fakes for this vendor only (both SDK generations)
/silicon_labs/footprint/                         flash/RAM measurement for this vendor only (both SDK generations)
/ti/gpio/hal_port_ti_mspm0.c
/ti/i2c/hal_port_ti_mspm0_i2c.c
/ti/uart/hal_port_ti_mspm0_uart.c
/ti/spi/hal_port_ti_mspm0_spi.c
/ti/adc/hal_port_ti_mspm0_adc.c
/ti/timer/hal_port_ti_mspm0_timer.c       DL_TimerG — DL_TimerA reserved for future PWM
/ti/pwm/hal_port_ti_mspm0_pwm.c           DL_TimerA — the reservation put to use
/ti/tests/                                unit tests + fakes for this vendor only
/ti/footprint/                            flash/RAM measurement for this vendor only
/os_port_baremetal.c                      backlog — not written yet
/os_port_freertos.c                       backlog — not written yet
/examples/blink_uart_i2c/                 the portability-proof demo app — not written yet
/docs/
```

Each `<vendor>/tests/` holds that vendor's `fakes/`, `test_<peripheral>.c`
files, `Makefile`, `README.md`, and `TEST_REPORT.md`; each
`<vendor>/footprint/` holds that vendor's `Makefile`, `generate_report.sh`,
`README.md`, and `FOOTPRINT.md` — see "Unit tests and flash/RAM footprint
measurement" above for why these are per-vendor and not a shared top-level
folder.

Customers never touch a build system — files get dropped into whatever
IDE project they already have (STM32CubeIDE, IAR, Keil, MCUXpresso,
Simplicity Studio, Code Composer Studio), gated by exactly one
preprocessor macro they define in their IDE's existing settings
(`TARGET_STM32`, `TARGET_NRF52`, `TARGET_SILABS`,
`TARGET_SILABS_SLGPIO`, `TARGET_TI_MSPM0`).
Forgetting to define it is a compile-time `#error`, never a silent
no-op.

CI (GitHub Actions + Renode) exists to verify the repo builds across
targets automatically — customers never need it and never see it as a
dependency.

## Decisions log

- MIT license (matches lwIP/littlefs/CMSIS convention; no CLA needed for
  a solo-maintained project)
- Repo name: placeholder `mcu-hal` — rename any time before public launch,
  low cost since nothing depends on it yet
- ESP32 dropped from scope: Xtensa toolchain/ABI/startup shape shares
  nothing with the Cortex-M vendors, so the wrapper has little leverage
  there, and Arduino's `digitalWrite`/`pinMode` already covers the
  hobbyist case (though not the production ESP-IDF case — that gap is
  real, just not this project's month-1 target)
- NXP deprioritized in favor of Silicon Labs + TI (user call)
- Peripheral scope for month 1: GPIO, UART, I2C, Timer/PWM only. SPI and
  ADC deferred — most vendor-specific quirks (DMA-driven SPI especially)
  live there. Both were later pulled forward at the user's explicit
  request (see the SPI and ADC sections above) — PWM remains deferred.
- Unit tests and footprint measurement, requested only after all 25
  adapters existed, were built out to full coverage immediately (all 25
  files, not a one-vendor pilot) rather than staged — same "every
  peripheral, every vendor" bar the adapters themselves were held to.
  Unity over Ceedling/CMock: this project has no real vendor SDK headers
  installed to auto-generate mocks from anyway, so hand-written fakes
  were required regardless, and Unity alone (no CMock, no Ruby/rake
  build) is the simpler dependency for a project already committed to
  "no build system customers must adopt."
- Tests and footprint were initially built under shared top-level
  `tests/`/`footprint/` folders, then reorganized per vendor
  (`<vendor>/tests/`, `<vendor>/footprint/`) after the user pointed out
  that a customer copying only `stm/` had no reason to receive nRF52/
  Silicon-Labs/TI test and footprint content too — the same "copy only
  what you need" principle the adapters themselves were already built
  around. Only `unity/` stayed shared at the repo root, since it's a
  third-party dependency, not vendor-specific test content.
- Timer's vendor adapters split Silicon Labs into two files again
  (classic `TIMER` vs. current `sl_sleeptimer`), same as GPIO/I2C/ADC —
  but for a different reason than those: it's not that the underlying
  hardware differs (it doesn't necessarily), it's that `sl_sleeptimer`
  is architecturally a software timer *service* over a shared hardware
  timer, not a "pick the vendor's Nth peripheral" driver the way every
  other adapter in this repo works — a genuinely new shape, not just a
  new vendor quirk.
- Silicon Labs' GPIO adapters (both `hal_port_silabs.c` and
  `hal_port_silabs_slgpio.c`) were missing their peripheral clock enable
  (`CMU_ClockEnable(cmuClock_GPIO, ...)` / `sl_gpio_init()`) — found by
  integrating against Silicon Labs' own `sl_simple_led` reference driver,
  which does this itself before its first GPIO call. STM32's GPIO
  adapter already handles this the right way; Silicon Labs' didn't, and
  the fake-header unit tests couldn't have caught it since the fakes
  don't model real clock-gating behavior. A full audit of every other
  Silicon Labs adapter against the real SDK source followed: the same
  gap was real and fixed in classic UART/I2C/ADC/Timer and the current
  SDK's IADC and `sl_sleeptimer` — but SPIDRV (both target macros' SPI)
  and the current SDK's `sl_i2c_init()` were verified, by reading their
  actual source, to already enable their own clock internally, so those
  two were correctly left untouched rather than "fixed" on a guess. See
  "Bug fix: Silicon Labs adapters weren't enabling their peripheral
  clocks" for the full per-file breakdown.
- PWM's API reuses GPIO's `(port << 5) | pin` scalar-encoding precedent
  rather than introducing a second opaque type for "channel" —
  `hf_pwm_t` is `(timer_instance << 2) | channel`. This is the first
  peripheral in this header with a genuine two-level hardware shape
  (one shared frequency, N independent-duty channels), and the scalar
  encoding turned out to generalize cleanly rather than needing a new
  pattern. Duty cycle uses millipercent (0-100000), not a raw
  compare-register value (no single number means the same thing across
  all four vendors' differently-sized counters) or a plain 0-100
  percent (too coarse for a smooth fade, and would make this project
  inherit Silicon Labs current SDK's own resolution ceiling as its
  whole API's ceiling rather than as one documented vendor asymmetry).
  Confirmed Silicon Labs classic (`TARGET_SILABS`) is the one vendor
  where `hf_timer_t` and the not-yet-implemented `hf_pwm_t` can
  genuinely collide on the same physical TIMER hardware — every other
  vendor is disjoint either by hardware or by a design choice already
  made when Timer's adapters were written (TI's DL_TimerG vs.
  DL_TimerA in particular, reserved in advance for exactly this).
  Design-only pass, same staging Timer went through — no vendor
  adapters yet, `HF_ENABLE_PWM` stays `0`.
- PWM's five vendor adapters were implemented immediately after the
  design pass, same "design first, adapters as a separate follow-up
  request" staging Timer went through. Every design-pass finding held
  up once written as real code, plus two new ones: Silicon Labs classic
  needed real, per-device-generation pin-routing register code
  (`GPIO->TIMERROUTE`/`ROUTEEN`) that classic emlib's `TIMER_InitCC()`
  itself doesn't provide — implemented the newer Series 2 scheme only
  (confirmed via Silicon Labs' own `sl_pwm.c` source), flagged the
  other two known device-generation schemes as EDIT ME rather than
  guessing; and nRF52's assumed `NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY`
  macro turned out not to exist, caught by a follow-up fetch during
  implementation and replaced with a project-defined constant, same
  precedent `hf_timer_t`'s nRF52 adapter already set. `HF_ENABLE_PWM`
  flipped to `1`.
