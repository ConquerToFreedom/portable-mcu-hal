# Development plan — staged, one phase at a time

Rule for every stage below: nothing in the next stage starts until the
current stage's "Definition of done" is actually met and confirmed, not
just drafted. Stop at `[VERIFY]` markers and wait for explicit go-ahead.

## Stage 0 — Decisions locked, repo scaffolded [DONE]
Name (placeholder `mcu-hal`), MIT license, repo skeleton with
`/include`, `/examples`, `/docs`, and one folder per vendor
(`stm/`, `nrf/`, `silicon_labs/`, `ti/`) split into `gpio/`/`i2c/`/etc.
subfolders per peripheral — not a flat `/src/`, see
`docs/architecture.md`'s "Repo layout". No implementation logic.

## Stage 1 — API freeze: GPIO only [DONE, re-scoped]
Originally scoped as GPIO + UART + I2C + PWM + os_port.h together;
re-scoped to GPIO only so a real vendor adapter can be proven before
sinking design time into signatures for peripherals nobody has
implemented yet. UART, I2C, PWM, and `os_port.h` are deferred to
Stage 4 below — do not write adapter code against them before then.

**[VERIFY] — done.** `include/hal_wrapper.h` GPIO section reviewed
against `docs/gpio-api-reference.md` and `docs/architecture.md`:
opaque `hf_gpio_t` scalar, `HF_PIN(port, num)` encoding, the five
`hf_gpio_mode_t` variants, and the four `hf_gpio_init`/`write`/`read`/
`toggle` signatures match the frozen design with no gaps. Frozen —
changing these signatures now means redoing adapter work.

Naming note: the public API prefix is `hf_`/`HF_`, not `hal_`/`HAL_` —
STM32's own SDK is literally called "STM32 HAL", so a shared `hal_`
prefix made our functions and STM32's (`HAL_GPIO_WritePin`, etc.) easy
to confuse side-by-side in the same adapter file. Renamed before Stage 2
code accumulated on top of it.

## Stage 2 — STM32 adapter, real hardware
Implement `stm/gpio/hal_port_stm32.c` (GPIO only) against STM32CubeIDE/HAL
behind `TARGET_STM32`. Build a blink-only example in
`examples/blink_uart_i2c/` and run it on the Black Pill. UART echo and
I2C read are added to this same example after Stage 4 freezes those
APIs — the folder name is kept so the example doesn't need renaming
twice.

**[VERIFY]** — Definition of done: example app boots and runs correctly
on real STM32 hardware. Business-logic code in the example never
references an ST-specific type directly.

Status: `stm/gpio/hal_port_stm32.c` written (`hf_gpio_init`/`write`/`read`/
`toggle` calling STM32 HAL's `HAL_GPIO_Init`/`WritePin`/`ReadPin`/
`TogglePin`, port table decoded per `docs/gpio-api-reference.md`'s
bitmask pattern). Not yet verified on
real hardware or even compiled against a real STM32Cube project — this
remote environment has no board and no STM32Cube SDK installed. Building
the example app and flashing the Black Pill is still open.

## Stage 3 — nRF52 adapter: the portability proof
Implement `nrf/gpio/hal_port_nrf52.c` (GPIO only) against nrfx/nRF5 SDK
behind `TARGET_NRF52`. Recompile the *identical* blink example with zero
edits outside the target macro. Verify the toggle-register question
flagged in `gpio-api-reference.md` before writing `hf_gpio_toggle` for
this vendor.

**[VERIFY]** — Definition of done: same example source, two vendors, two
working binaries. Any place the app needs a change is a leak in Stage 1's
API — fix the header, don't patch around it in the example.

Status: `nrf/gpio/hal_port_nrf52.c` written. `hf_gpio_t`'s `(port<<5)|pin`
encoding matches Nordic's own `NRF_GPIO_PIN_MAP` exactly, so this adapter
needs no port-decode table, unlike STM32's. Toggle-register question
resolved: nRF52 GPIO has no atomic toggle register, confirmed against
Nordic's own `nrf_gpio_pin_toggle()` (a read-modify-write of `OUT`) —
the adapter calls that SDK function directly. Not yet verified on real
hardware or against a real nRF5 SDK/nrfx build — this remote environment
has neither an nRF52 DK nor the SDK installed.

## Stage 4 — UART/I2C/PWM/SPI/ADC + os_port.h API freeze
Design and freeze signatures for UART, I2C, PWM, and the `os_port.h`
RTOS-shim contract — deferred out of Stage 1 so GPIO could reach real
hardware first. No vendor implementation yet, same as original Stage 1:
headers compile standalone with stub `.c` bodies that just return. SPI
and ADC were added to this stage's scope later, pulled forward from the
month-2 backlog at the user's explicit request — see their own status
blocks below.

**[VERIFY]** — Definition of done: headers reviewed and frozen, same bar
Stage 1 used for GPIO.

**UART portion — done.** `hf_uart_t` (opaque 0-based instance index,
same lookup-table pattern as GPIO's port), `hf_uart_init`,
`hf_uart_write` (blocking, no timeout param), `hf_uart_read`
(non-blocking, returns actual byte count) added to
`include/hal_wrapper.h`. Fixed at 8N1/no-flow-control for v1; pin
muxing explicitly out of scope, same division of responsibility GPIO
already has on TI MSPM0's IOMUX. Full design rationale — including why
no timeout parameter was needed and how that sidesteps an ordering
dependency on `os_port.h` (not designed yet) — is in
`docs/architecture.md`'s new UART section. Verified the header still
compiles standalone against a throwaway stub implementation with
`gcc -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion`, zero
warnings — cheap to do since `hal_wrapper.h` has no vendor
dependencies, and worth doing every time this file changes.

Extended the same day, before moving on: the blocking-only design above
had a real gap — no interrupt/DMA-driven tier, which means no way to
do a non-blocking write or get notified when data arrives, forcing
anyone who needed that back to raw vendor SDK calls. Added
`hf_uart_write_async`, `hf_uart_set_tx_callback`, and
`hf_uart_set_rx_callback` — additive, nothing above changed. Which
underlying mechanism (interrupt vs. DMA) each adapter uses for these
stays an adapter/Stage-5 implementation choice, documented per vendor
same as the toggle and pull/open-drain asymmetry tables; DMA-channel
selection itself stays out of the public API on purpose, same "thin
contract" reasoning as pin muxing. Recompiled the stub with the new
declarations added — still zero warnings. See
`docs/architecture.md`'s "UART, part two" section for the full
reasoning. I2C, PWM, and `os_port.h` are still undesigned; this stage
isn't done until those are too.

**UART vendor adapters — implemented for all five**, at the user's
explicit request, same pattern as I2C. Every adapter needs a
background receive path regardless of whether the async tier is used
(`hf_uart_read()`'s own contract requires it), so all five carry the
same 128-byte ring buffer drained only by `hf_uart_read()` and filled
only by a per-vendor interrupt handler. Wrote
`stm/uart/hal_port_stm32_uart.c`, `nrf/uart/hal_port_nrf52_uart.c`,
`silicon_labs/uart/hal_port_silabs_uart.c`,
`silicon_labs/uart/hal_port_silabs_slgpio_uart.c`,
`ti/uart/hal_port_ti_mspm0_uart.c`. Verified STM32 HAL and nRF52's
`nrfx_uarte.h` (GitHub source fetch — found its continuous-reception
model is buffer-chained, not per-byte, so this adapter ping-pongs two
1-byte buffers to get the same granularity as the other vendors) and
Silicon Labs' emlib `em_usart.h` before writing each. Found a genuine
architectural gap on Silicon Labs' current-driver side: unlike
`sl_gpio`/`sl_i2c`, there's no independent "current" UART driver —
`sl_iostream_usart` is confirmed (direct source read) to wrap the same
classic emlib peripheral, with no confirmed-stable public callback or
non-blocking-read primitive, and mixing it with a hand-rolled RX
interrupt would risk two pieces of code fighting over the same vector.
That adapter deliberately uses the same raw emlib calls as the classic
one instead — documented as a finding, not a shortcut, in
`docs/architecture.md`'s new "UART, part three" section (which also
covers the per-vendor differences and TI's flagged, not-independently-
confirmed constant names, same honesty policy as its I2C adapter).

Also fixed a real, vendor-agnostic design gap while implementing:
`hf_uart_tx_callback_t` carries no success/failure signal (UART error
reporting was scoped out of v1 entirely), so every adapter clears its
busy flag on a transfer error without invoking the callback — firing
it would incorrectly imply success. Applied consistently across all
five rather than left as an accidental STM32-only fix.

Sanity-checked all five new files for balanced braces/parens, same
verification level as I2C (full compilation needs each vendor's real
SDK, not available here). Flipped `HF_ENABLE_UART` to 1 in
`hal_wrapper_config.h` now that real adapters exist. None of the five
UART adapters have been compiled against a real SDK or run on real
hardware — same standing gap as every adapter in this repo.

**I2C portion — designed AND implemented for all five adapters in one
pass**, at the user's explicit request ("write for all controllers").
This jumps ahead of the plan's own stated order (Stage 4 = design
only, Stage 5 = vendor implementation) deliberately — same kind of
re-scoping this plan has already done twice (GPIO pulled ahead of
UART/I2C/PWM in Stage 1; Silabs/TI GPIO adapters pulled ahead of
Stage 4/5's original sequence). `hf_i2c_t`, `hf_i2c_init`,
`hf_i2c_write`/`read`/`write_read`, and the async tier
(`hf_i2c_write_async`/`read_async` + tx/rx callbacks) added to
`include/hal_wrapper.h` — same opaque-instance-plus-lookup-table shape
as UART, `bool` success/fail return instead of a byte count (I2C is
transactional, not a stream), and `success`-carrying async callbacks
(a NACK is routine I2C operation, not a rare fault). Full rationale,
including the real repeated-START handling differences across all
five adapters and two newly-discovered per-vendor asymmetries (STM32's
advisory-only clock_hz; nRF52/current-Silabs' explicit SCL/SDA pin
tables), is in `docs/architecture.md`'s new I2C section — not
duplicated here.

Wrote all five adapters: `stm/i2c/hal_port_stm32_i2c.c`,
`nrf/i2c/hal_port_nrf52_i2c.c`, `silicon_labs/i2c/hal_port_silabs_i2c.c`,
`silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c`,
`ti/i2c/hal_port_ti_mspm0_i2c.c`.
Verified vendor signatures directly against real source before writing
each — STM32 HAL, nrfx's `nrfx_twim.h`, emlib's `em_i2c.h`, and
Silicon Labs' current `sl_i2c.h` (fetched from
`SiliconLabs/simplicity_sdk` `sisdk-2025.6` on GitHub, same as the
`sl_gpio` adapter) were all checked against real header source. TI's
adapter is the one exception: TI's docs domain and community forum
are both blocked by this environment's network egress proxy, so two
specific constant names (an RX-FIFO-ready status flag, and the exact
`DL_I2C_IIDX_CONTROLLER_*` interrupt constants) rely on driverlib's
established naming pattern rather than a confirmed source read —
flagged explicitly in that file rather than presented with unearned
confidence.

Found and fixed a real bug in my own first draft of
`hf_i2c_read_async()` on TI MSPM0 before it ever got committed: it
started the transfer and reported completion without ever copying the
received bytes into the caller's buffer, silently violating what the
header itself promises. Fixed by stashing the buffer pointer/length
per instance and draining the RX FIFO inside the IRQ handler.

Sanity-checked all five files for balanced braces/parens (no way to
compile these against real vendor SDKs in this environment, unlike
`hal_wrapper.h` itself, which has none of these files' vendor
dependencies). Real compilation and hardware verification remain open
for all five — same standing gap as every adapter in this repo so far.
PWM and `os_port.h` are still undesigned; Stage 4 isn't done until
those are too, and none of Stage 2/3/5's [VERIFY]s can close until
real hardware testing happens for any of this.

**SPI portion — designed (header only, no vendor adapters yet)**, at
the user's explicit request — pulled forward from the month-2 backlog
(originally grouped with ADC as carrying the most vendor-specific
quirks). `hf_spi_t`, `hf_spi_init` (with an `hf_spi_mode_t` CPOL/CPHA
enum, SPI's own universal mode-0-through-3 numbering, not a new one),
`hf_spi_write`/`read`/`transfer`, and the async tier
(`write_async`/`read_async` + parameterless tx/rx callbacks) added to
`include/hal_wrapper.h`. Chip-select composes the already-frozen GPIO
API directly (`hf_gpio_t cs_pin` on every call, asserted/deasserted by
the adapter around each transfer) rather than inventing a new concept
— the first place two of this wrapper's own peripheral APIs compose.
`hf_spi_transfer` (full-duplex, SPI's actual native operation) is the
real primitive; `write`/`read` are its degenerate single-direction
cases, offered as named functions anyway for the common case. `bool`
return like I2C's, but for a different reason (SPI has no NACK-style
bus failure at all, so `false` only ever means an invalid instance/
pin). Parameterless callbacks like UART's, not success-carrying like
I2C's, same reasoning: no NACK-equivalent for a success flag to report.
`hf_spi_transfer`'s async equivalent deferred, same reasoning as
`hf_i2c_write_read`'s deferral. Verified the header still compiles
standalone against an expanded stub with `gcc -std=c99 -Wall -Wextra
-Wpedantic -Wsign-conversion`, zero warnings. Full rationale in
`docs/architecture.md`'s new SPI section.

**SPI portion — vendor adapters implemented for all five targets**,
following the same "write for all controllers" pattern already applied
to I2C and UART. `HF_ENABLE_SPI` flipped to `1` in
`hal_wrapper_config.h`. The chip-select-via-GPIO design was confirmed
in practice — every adapter asserts/deasserts `cs_pin` via the
already-frozen `hf_gpio_write()` with zero vendor-specific CS handling
needed. STM32 uses `HAL_SPI_Transmit`/`Receive`/`TransmitReceive` plus
the `_IT` variants and weak completion callbacks, requiring CubeMX's
NSS management set to Software/disabled. nRF52's `nrfx_spim` maps onto
this API unusually cleanly via its `NRFX_SPIM_XFER_TRX`/`TX`/`RX`
descriptor macros. Silicon Labs has no dedicated `sl_spi` driver, but
`platform/emdrv/spidrv` (SPIDRV) genuinely supports both blocking and
real-callback async, so it's used unchanged for both `TARGET_SILABS`
and `TARGET_SILABS_SLGPIO` — and a real hardware-sharing hazard was
found and documented: classic SPI (via SPIDRV) and classic UART share
the same USART peripheral/IRQ vectors, so enabling both adapters for
the same instance is a duplicate-symbol link error (cross-referenced in
all four affected files). TI's SPI adapter was verified against the
official `TexasInstruments/mspm0-sdk` GitHub mirror directly — a
meaningfully higher-confidence source than the web-search snippets the
I2C/UART TI adapters had to rely on, since TI's docs domain and forum
are both blocked in this environment; two names
(`DL_SPI_getPendingInterrupt`, `DL_SPI_disableInterrupt`) remain
flagged as inferred-from-naming-pattern rather than confirmed. Sanity-
checked all five new files for balanced braces/parens, same as every
prior adapter batch. Full per-vendor rationale in
`docs/architecture.md`'s new "SPI, part two" section. Real compilation
and hardware verification remain open for all five — same standing gap
as every adapter in this repo.

**ADC portion — designed (header only, no vendor adapters yet)**, at
the user's explicit request — pulled forward from the month-2 backlog
(originally grouped with SPI as carrying the most vendor-specific
quirks). `hf_adc_t`/`hf_adc_channel_t`, `hf_adc_init`,
`hf_adc_read`, and the async tier (`hf_adc_read_async` + one
parameterless `hf_adc_callback_t`, not a tx/rx pair, since ADC is
receive-only) added to `include/hal_wrapper.h`. Channel is the vendor's
own channel number, not remapped through `hf_gpio_t` — channel-to-pin
routing is a fixed silicon mapping on all four vendors, unlike a
digital alternate-function mux. Fixed 12-bit resolution for v1 (widest
common denominator across all four vendors) rather than a configurable
enum. `bool` return for the same reason as SPI's, not I2C's: no
bus-level partial-failure mode exists for a single conversion. Verified
the header still compiles standalone against an expanded stub with
`gcc -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion`, zero
warnings. Full rationale in `docs/architecture.md`'s new ADC section.

**ADC portion — vendor adapters implemented, five files across four
vendors**, following the same "write for all controllers" pattern
already applied to I2C, UART, and SPI. `HF_ENABLE_ADC` flipped to `1`
in `hal_wrapper_config.h`. Unlike GPIO/I2C/UART/SPI (one file per
vendor, or a shared file for Silicon Labs on UART/SPI), Silicon Labs
needed **two real adapters here**: classic ADC and IADC (current-
generation) are genuinely different hardware peripheral blocks, not
different driver layers over the same registers — the same
two-adapter shape as this vendor's GPIO/I2C split, not UART/SPI's
one-shared-driver shape. STM32 reconfigures the single regular-
sequence channel via `HAL_ADC_ConfigChannel()` on every call and uses
`HAL_ADC_Start_IT`/weak completion callbacks for async; resolution is
advisory only since calling `HAL_ADC_Init()` itself would re-run
CubeMX's whole peripheral configuration. nRF52's `nrfx_saadc` turned
out to have a genuine native blocking mode (`nrfx_saadc_simple_mode_set`
with a NULL event handler blocks until done) — the opposite finding
from this vendor's I2C/UART/SPI adapters, none of which have a
NULL-handler blocking mode. Classic Silicon Labs ADC has a real,
part-family-dependent channel-selection wrinkle (`input`/
`ADC_SingleInput_TypeDef` on non-APORT parts vs. `posSel`/
`ADC_PosSel_TypeDef` on APORT parts); this adapter uses the simpler
non-APORT field with an EDIT-ME note. IADC (current) carries more
genuinely-unconfirmed specifics than any other adapter in this repo —
several struct/macro names follow Silicon Labs' published IADC
example-project conventions rather than a directly re-quoted struct
definition, flagged explicitly and pointed at Simplicity SDK's own
"iadc_single_conversion" examples for cross-check. TI's ADC12 results
are indexed by a conversion-memory slot (`DL_ADC12_MEM_IDX`), not read
back by raw channel number — this adapter deliberately reconfigures
MEM_IDX 0 to the requested channel on every call, since `hf_adc_channel_t`
needs to select an arbitrary channel per call; six of
`DL_ADC12_configConversionMem()`'s enum-constant arguments weren't
independently confirmed and use conservative disabled-style defaults,
flagged in the file. Every adapter's error path clears the busy flag
without invoking the parameterless callback, same "no false success"
rule as UART/SPI. Sanity-checked all five new files for balanced
braces/parens. Full per-vendor rationale in `docs/architecture.md`'s
new "ADC, part two" section. Real compilation and hardware
verification remain open for all five — same standing gap as every
adapter in this repo.

## Unit tests and flash/RAM footprint measurement — all 25 adapters [DONE]

Requested after all 25 adapter files (5 peripherals × 5 target macros) existed:
a real unit test per adapter and a real per-adapter flash/RAM footprint
number. Built to full coverage in one pass, not staged on a single vendor
first — 153 Unity tests across 25 files, each compiled with the host's own
`gcc` against a hand-written fake of the real vendor SDK header(s) that
adapter includes, plus a real ARM Cortex-M cross-compiler footprint
measurement (`arm-none-eabi-gcc -Os`, per-vendor-real-core: Cortex-M4 for
STM32/nRF52/classic Silicon Labs, Cortex-M33 for current Silicon Labs,
Cortex-M0+ for TI).

**Segregated per vendor, not one shared top-level folder.** Initially built
as top-level `tests/`/`footprint/` covering all five vendors — the user
then pointed out that a customer copying only `stm/` had no reason to
receive nRF52/Silicon-Labs/TI test and footprint content, the same
"copy only what you need" principle the adapters themselves already
follow. Reorganized to `<vendor>/tests/` and `<vendor>/footprint/` for
each of the four vendor folders, each self-contained: `make -C stm/tests
report`, `make -C nrf/footprint report`, etc. Only `unity/` (vendored
Unity, MIT) stayed shared at the repo root, since it's a third-party
dependency like `include/hal_wrapper.h`, not vendor-specific test content.
Each vendor's `TEST_REPORT.md`/`FOOTPRINT.md` is committed as a
point-in-time snapshot, regenerated the same way.

All 25 adapters pass (153/153: 34 STM32, 32 nRF52, 56 Silicon Labs [10
files, both SDK generations], 31 TI). Writing the tests surfaced one
genuine bug along the way: `nrf/uart/hal_port_nrf52_uart.c` calls
`nrfx_uarte_init()` without the explicit context argument this vendor's
I2C/SPI adapters both pass, so multi-instance UARTE event attribution is
unverified — flagged, not silently fixed (fixing it needs confirming real
nrfx's actual context-delivery mechanism first, which is out of scope for
a test-writing pass). Full design writeup, including the
STM32-callable-IRQHandler vs. nRF52-registered-event-handler distinction
that shaped the fakes, is in each vendor's `tests/README.md` and
`docs/architecture.md`'s section of the same name.

Getting a working ARM cross-compiler here was itself a real obstacle: this
environment's `apt` mirror throttled the ~460 MB
`libstdc++-arm-none-eabi-newlib` package badly enough that a prebuilt
xPack GCC 13.3.1 release fetched directly from GitHub was used instead
(~70x faster) — see any vendor's `footprint/README.md` for the exact
recovery steps, in case a future environment hits the same slow-mirror
problem.

## Unit tests and flash/RAM footprint measurement for Timer and PWM — all 35 adapters [DONE]

Requested once Timer and PWM's vendor adapters existed, extending the same
per-vendor infrastructure above rather than rebuilding it: 10 new adapter
files (Timer × 5, PWM × 5) got the identical treatment. 64 new Unity tests
across 10 new test files (11 STM32, 15 nRF52, 26 Silicon Labs [4 files,
both SDK generations], 12 TI), plus the same real `arm-none-eabi-gcc`
footprint measurement. This brings the wrapper to 35 total adapter files,
217 total tests, all passing.

Each vendor's new fakes had one genuinely new complication, not just more
of the same boilerplate: STM32 needed a shared `TIM_HandleTypeDef` (Timer
and PWM are the same real peripheral family) with real, non-sequential
`TIM_CHANNEL_1..4` values; nRF52's PWM fake had to model EasyDMA sequence
playback rather than a register write, and in the process caught a real
research error from the design pass — `NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY`
isn't a real nrfx macro, fixed with a project-defined constant; TI needed a
shared `GPTIMER_Regs` type across `DL_TimerG`/`DL_TimerA`; Silicon Labs
classic needed an entirely new `GPIO` peripheral-level fake struct for the
`GPIO->TIMERROUTE`/`ROUTEEN` pin-routing registers its PWM adapter pokes
directly (with one real, documented fake limitation: Silicon Labs' `_SET`
register alias has hardware OR semantics this fake's plain struct field
can't reproduce); Silicon Labs current needed slot-based (not
pointer-based) test accessors since `sl_pwm_instance_t` is adapter-private
static state. Full per-vendor detail in `docs/architecture.md`'s "Unit
tests and flash/RAM footprint measurement for Timer and PWM" section and
each vendor's `tests/TEST_REPORT.md`.

## Timer portion — vendor adapters implemented for all five targets

The last two peripherals left in Stage 4's scope, requested in this order:
periodic timer first, PWM after. `hf_timer_t` is frozen in
`include/hal_wrapper.h` — `hf_timer_init(instance, period_us, mode)` +
`hf_timer_start`/`hf_timer_stop` + one parameterless callback
(`hf_timer_callback_t`), covering both one-shot and periodic modes. Kept
as a genuinely separate peripheral from the not-yet-designed `hf_pwm_t`
(independent instance-number spaces), since nRF52 backs periodic counting
(`nrfx_timer`/`nrfx_rtc`) and PWM (`nrfx_pwm`) with entirely different
hardware, unlike STM32/Silicon Labs/TI where one physical timer block can
serve either role.

Vendor adapters written for all five target macros immediately after the
design (`stm/timer/hal_port_stm32_timer.c`,
`nrf/timer/hal_port_nrf52_timer.c`, both Silicon Labs adapters, and
`ti/timer/hal_port_ti_mspm0_timer.c`), researched via direct GitHub source
fetches the same way as every prior implementation pass. Structural
finding: Silicon Labs needed two files again, but for a new *reason* this
time — not different hardware (classic `TIMER` and current
`sl_sleeptimer` don't necessarily target different silicon the way classic
ADC vs. IADC do), but a different API *shape*: `sl_sleeptimer` is a
software timer service where one shared hardware timer backs an
effectively unlimited number of independent handles, not a "pick the
vendor's Nth peripheral" driver. Real per-vendor findings: STM32's
basic timers (TIM6/7) needed software-enforced one-shot (STM32's own
One-Pulse Mode is an output-channel feature, not a fit for a headless
timer); nRF52's `period_us` is genuinely accurate (not advisory, unlike
this vendor's other adapters) since this project owns the timer's clock
configuration outright; classic Silicon Labs and TI both have a genuine
hardware one-shot feature (`oneShot` field / `DL_TIMER_TIMER_MODE_ONE_SHOT`)
needing no software enforcement; current-generation Silicon Labs has a
real, permanent resolution loss — `sl_sleeptimer` only exposes
millisecond-granularity conversion, so `period_us` rounds up to whole
milliseconds on this vendor specifically. Full per-vendor rationale in
`docs/architecture.md`'s new "Timer, part two" section.

Sanity-checked all five new files for balanced braces/parens and verified
`hal_wrapper.h` still compiles standalone for every target macro.
`HF_ENABLE_TIMER` flipped to `1` in `hal_wrapper_config.h`. As with every
adapter in this repo, none of the five have been compiled against a real
SDK or run on real hardware — TI MSPM0 and Silicon Labs' current
`sl_sleeptimer` adapter carry the most individually-flagged unconfirmed
specifics of the five and are the highest-priority to verify first.

## PWM portion — vendor adapters implemented for all five targets

The second half of the "periodic timer first, PWM after" pair. `hf_pwm_t`
is frozen in `include/hal_wrapper.h` — `hf_pwm_init(channel, freq_hz)` +
`hf_pwm_start`/`hf_pwm_stop` + `hf_pwm_set_duty(channel, duty_millipercent)`.
Researched all five vendors' real PWM APIs first, same discipline as
Timer: STM32's `HAL_TIM_PWM_*` family, nRF52's `nrfx_pwm`, Silicon Labs
classic's `TIMER_InitCC`/compare-channel API and the current SDK's
`sl_pwm` driver, and TI's `DL_Timer_initFourCCPWMMode`/
`DL_Timer_setCaptureCompareValue`.

Structural finding, confirmed on every vendor: PWM is genuinely
two-level hardware (one timer, one shared frequency, N independent-duty
channels) — the first peripheral in this header with that shape.
Reused GPIO's `(port << 5) | pin` scalar-encoding precedent rather than
adding a second opaque type: `hf_pwm_t` is
`(timer_instance << 2) | channel`. Duty cycle is `duty_millipercent`
(0-100000), not a raw compare-register value (no single number means
the same thing across STM32/nRF52/Silicon Labs/TI's differently-sized
counters) and not a plain 0-100 percent (too coarse for a smooth fade).

Real per-vendor findings: STM32's basic timers (TIM6/TIM7, already
claimed by `hf_timer_t`) have no output-compare channels at all, so PWM
is forced onto a different, general-purpose timer regardless — disjoint
by hardware, not convention. nRF52's `nrfx_pwm` is a genuinely more
involved, EasyDMA sequence-player shape, not a simple compare-register
write — a real implementation wrinkle (the duty-value array must live
in a persistent RAM buffer, not a stack variable, since EasyDMA reads
it continuously while playing) to handle when the adapter is written,
not a design-level blocker. Silicon Labs classic (`TARGET_SILABS`) is
the one vendor where `hf_timer_t` and `hf_pwm_t` genuinely can collide —
both use the same emlib `TIMER_TypeDef` family, and `hf_timer_t`
already claims TIMER0/TIMER1, so the PWM adapter must use different
TIMERn instances (verify your part has enough). Silicon Labs current
(`TARGET_SILABS_SLGPIO`) gets a third shape again: `sl_pwm` is a real
current-generation driver, but confirmed (its own struct holds a
`TIMER_TypeDef *`) to sit directly on classic TIMER hardware, not a
genuinely different peripheral — used anyway (it's what Silicon Labs
ships and documents for this generation), with its one real cost
flagged: `sl_pwm_set_duty_cycle()` only takes a plain 0-100 percent, so
`duty_millipercent` is rounded to the nearest whole percent on this
target specifically, a real permanent resolution loss. TI's `DL_TimerA`
was already reserved for exactly this when `hf_timer_t`'s adapter chose
`DL_TimerG` instead — disjoint by design, no new finding needed there.

Full per-vendor rationale in `docs/architecture.md`'s "Public API
surface (v1, PWM — Stage 4)" section.

Vendor adapters written for all five target macros immediately after
the design (`stm/pwm/hal_port_stm32_pwm.c`,
`nrf/pwm/hal_port_nrf52_pwm.c`, both Silicon Labs adapters, and
`ti/pwm/hal_port_ti_mspm0_pwm.c`), same "adapters as a separate
follow-up request" staging Timer went through. Every design-pass
finding held up once written as real code, plus two new ones that only
surfaced during implementation: Silicon Labs classic needed real
pin-routing register code (`GPIO->TIMERROUTE`/`ROUTEEN`) that classic
emlib's `TIMER_InitCC()` doesn't provide on its own — implemented the
newer Series 2 scheme only (confirmed via Silicon Labs' own `sl_pwm.c`
source), with the other two known device-generation schemes flagged as
EDIT ME rather than guessed at; and nRF52's assumed
`NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY` macro turned out not to exist,
caught by a follow-up fetch and replaced with a project-defined
constant, same precedent `hf_timer_t`'s nRF52 adapter already set with
`HF_NRF52_TIMER_IRQ_PRIORITY`. Full per-vendor rationale in
`docs/architecture.md`'s new "PWM, part two: all five vendor adapters"
section.

Sanity-checked all five new files for balanced braces/parens/brackets
and verified `hal_wrapper.h` still compiles standalone for every target
macro. `HF_ENABLE_PWM` flipped to `1` in `hal_wrapper_config.h`. As with
every adapter in this repo, none of the five have been compiled against
a real SDK or run on real hardware — Silicon Labs classic's pin-routing
scheme uncertainty and nRF52's structurally different EasyDMA shape are
the highest-priority to verify first.

## Stage 5 — Silicon Labs adapter, RTOS port, CI, packaging
Implement `silicon_labs/gpio/hal_port_silabs.c` (emlib) behind `TARGET_SILABS` —
confirms generality across a third vendor family, and is the simplest of
the three to implement (port+index, not bitmask). Extend the STM32 and
nRF52 examples with the UART echo + I2C read pieces now that Stage 4 has
frozen those APIs. Add `os_port_baremetal.c` and `os_port_freertos.c`.
Wire up Renode-based CI for STM32 and nRF52 (Silabs Renode support may be
thinner — verify, flag as known gap if so rather than skipping silently).
Finish README (STM32↔nRF52 swap as the hero demo), docs, and draft the
launch post.

**[VERIFY]** — Definition of done: a stranger clones the repo, sets one
macro, and has GPIO+UART working on any of the three targets within 30
minutes.

Status: `silicon_labs/gpio/hal_port_silabs.c` written GPIO-only, same re-scoping as
Stages 2/3 — done ahead of Stage 4's UART/I2C/PWM freeze because GPIO
across three vendors was prioritized. `GPIO_Port_TypeDef`'s enum values
(`gpioPortA`=0 .. `gpioPortF`=5) match `HF_PIN_PORT(pin)` directly, so
this adapter needs no lookup table, just a cast — simpler than STM32's
and even nRF52's. Along the way, found and fixed a bug in
`docs/gpio-api-reference.md`'s original reference table: it listed
`GPIO_PinOutGet()` for "Read", but that reads the DOUT (last-driven)
register — `GPIO_PinInGet()` (DIN, actual pad state) is the correct
match for what STM32's `HAL_GPIO_ReadPin` and nRF's `nrf_gpio_pin_read`
actually do. Not yet verified against a real Gecko SDK build or EFR32
hardware — same gap as the other two adapters. RTOS port, CI, and
packaging parts of this stage remain untouched.

Added a second, real Silabs adapter: `silicon_labs/gpio/hal_port_silabs_slgpio.c`
behind a new `TARGET_SILABS_SLGPIO` macro, targeting Silicon Labs'
current `sl_gpio` driver (Series 2 + Series 3) alongside the classic
emlib one (Series 0/1/2, no Series 3) — the two SDK generations are
genuinely incompatible APIs (struct-passed-by-pointer vs. two loose
args, different names, `sl_status_t` returns), not a rename, so this
needed a second file and a second target macro rather than an `#ifdef`
inside the existing one. Verified `sl_gpio_t`, `sl_gpio_mode_t`, and
the function signatures directly against
`platform/service/device_manager/inc/sl_device_gpio.h` and
`platform/driver/gpio/inc/sl_gpio.h` in `SiliconLabs/simplicity_sdk`
(`sisdk-2025.6`) before writing it — see
`docs/gpio-api-reference.md`'s emlib-vs-sl_gpio mapping table. Also
confirmed (and documented in `nrf/gpio/hal_port_nrf52.c` and the README) that
nRF52 needs no equivalent second adapter: nrfx's `nrf_gpio.h` is
shared unchanged between the legacy nRF5 SDK and the current nRF
Connect SDK, so the existing single adapter already covers both.

GPIO for the fourth and last vendor was pulled forward too, same
re-scoping as Silabs above: `ti/gpio/hal_port_ti_mspm0.c` written, gated by
the `TARGET_TI_MSPM0` macro that was already reserved in
`include/hal_wrapper.h`. Verified against TI's driverlib docs and the
Zephyr MSPM0 GPIO driver source before writing: `DL_GPIO_enableOutput`/
`disableOutput`/`setPins`/`clearPins`/`togglePins`/`readPins` all
operate on the same `GPIO_Regs*` + bitmask our other bitmask vendors
use, so those four are straightforward. But pull resistors, open-drain,
and drive strength live behind a *different* per-pin IOMUX `PINCM`
index (`DL_GPIO_initDigitalInputFeatures`/`OutputFeatures`) that has no
formulaic relationship to `hf_gpio_t`'s port+bit encoding — it's an
arbitrary per-chip lookup normally baked into SysConfig-generated
`ti_msp_dl_config.c`. `hf_gpio_init()` on this vendor can only ever set
direction; it returns `false` for `HF_GPIO_INPUT_PULLUP`/`PULLDOWN` and
`HF_GPIO_OUTPUT_OPENDRAIN` rather than silently doing half the job. See
the new pull/open-drain asymmetry table in `docs/architecture.md`
(same "document it, don't hide it" treatment as the toggle asymmetry
table). Not yet verified against a real MSPM0-SDK build or hardware —
same gap as every other adapter so far.

## Backlog (month 2, not month 1)
- A monotonic uptime/"clock" read (Arduino `millis()`-style) and a
  proper blocking delay primitive — deliberately left out of
  `hf_timer_t`, likely belongs in `os_port.h` once that's designed
- Timer mid-period elapsed/remaining-count read
- ADC configurable resolution, multi-channel scan mode, and continuous/
  free-running conversion (v1 is fixed 12-bit, single-channel,
  single-shot per call)
- NXP (deprioritized, not dropped)
- UART parity/stop-bits/flow-control configurability, if a real
  customer need surfaces (v1 is fixed 8N1, no flow control)
- SPI LSB-first bit order and active-high chip-select, if a real
  customer need surfaces (v1 is fixed MSB-first, active-low)
- SPI `hf_spi_transfer`'s async (full-duplex) equivalent
- Silicon Labs classic PWM's older Series 2 (per-timer
  `GPIO->TIMER0ROUTE[]`) and Series 0/1 (`TIMER_TypeDef`-level
  `ROUTEPEN`/`ROUTELOC0`) pin-routing schemes — only the newer, combined
  `GPIO->TIMERROUTE[]` scheme is implemented in
  `silicon_labs/pwm/hal_port_silabs_pwm.c`, flagged as EDIT ME for the
  other two rather than guessed at
- PWM inverted output polarity and center-aligned (up-down counting)
  mode, if a real customer need surfaces (v1 is fixed active-high,
  edge-aligned)
- PWM duty cycle/frequency readback
- PWM complementary/dead-time-inserted channel pairs (H-bridge
  motor-control driving) — out of scope for this project's "thin
  wrapper" ambitions unless that changes
