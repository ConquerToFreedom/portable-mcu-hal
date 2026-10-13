# Silicon Labs adapter unit tests

Each of this vendor's 16 adapter files (8 peripherals × 2 SDK generations) has a matching test
file here, built and run on the host — no vendor SDK or real hardware needed. Run everything
with:

```
make -C silicon_labs/tests report
```

or one binary at a time, e.g. `make -C silicon_labs/tests silabs_gpio && ./silicon_labs/tests/build/test_silabs_gpio`.

See `TEST_REPORT.md` for the last recorded full run.

## Layout

This vendor ships two genuinely different SDK generations behind two target macros — same split
as the adapter files themselves (`../gpio/hal_port_silabs.c` vs. `../gpio/hal_port_silabs_slgpio.c`):

- `test_silabs_<peripheral>.c` — classic emlib (`TARGET_SILABS`)
- `test_silabs_slgpio_<peripheral>.c` — current `sl_gpio`/`sl_i2c`/IADC (`TARGET_SILABS_SLGPIO`)
- `fakes/silabs/` — fake classic emlib (`em_gpio.h`, `em_usart.h`, `em_i2c.h`, `em_adc.h`,
  `spidrv.h`)
- `fakes/silabs_slgpio/` — fake current drivers (`sl_gpio.h`, `sl_status.h`, `sl_i2c.h`,
  `em_iadc.h`)

UART and SPI adapters under **both** target macros are functionally identical (same real shared
USART/SPIDRV driver — see those adapters' own header comments), so `test_silabs_slgpio_uart.c`
and `test_silabs_slgpio_spi.c` link `fakes/silabs`'s fakes, not anything slgpio-specific — see the
Makefile.

## Why this approach

Each adapter `.c` file `#include`s a real Silicon Labs SDK header that isn't installed in this
environment. The `fakes/` directories provide fake implementations of just the slice of each real
header these adapters actually call — same names/signatures/struct layouts, confirmed against
`SiliconLabs/simplicity_sdk` and `SiliconLabs/Gecko_SDK` during this project's research passes.
The adapter `.c` files themselves compile completely unchanged.

Adapters define their own non-static `USARTx_*_IRQHandler`/`I2Cx_IRQHandler`/`ADCx_IRQHandler`/
`IADC0_IRQHandler` functions — callable directly by a test to simulate an interrupt firing, same
pattern as this project's TI fakes.

## Known limitations

- **Host x86-64, not the real target architecture** — see `../footprint/` for the one place this
  project uses a real ARM Cortex-M cross-compiler.
- **The fakes are hand-written, not generated from real headers**, because no Simplicity SDK is
  installed in this environment. `fakes/silabs_slgpio/em_iadc.h` in particular backs the adapter
  with the most genuinely-unconfirmed specifics in this whole repo (see that adapter's own header
  comment) — a passing fake test here says nothing about whether those names are actually correct
  against your installed SDK.
- **A busy-flag "leak" across tests is a real trap**: adapter statics are process-global and Unity
  does not restart the process between `RUN_TEST()` calls within one binary. Every busy-rejection
  test here fires the completion interrupt/callback before it ends to avoid leaking a stuck flag
  into the next test.
- Coverage is broad, not exhaustive — see `TEST_REPORT.md`.
