# nRF52 adapter unit tests

Each of this vendor's 8 adapter files has a matching `test_<peripheral>.c` here, built and run on
the host — no vendor SDK or real hardware needed. Run everything with:

```
make -C nrf/tests report
```

or one binary at a time, e.g. `make -C nrf/tests gpio && ./nrf/tests/build/test_gpio`.

See `TEST_REPORT.md` for the last recorded full run.

## Why this approach

Each adapter `.c` file `#include`s nrfx's real headers (`nrf_gpio.h`, `nrfx_uarte.h`,
`nrfx_twim.h`, `nrfx_spim.h`, `nrfx_saadc.h`), which aren't installed in this environment.
`fakes/` provides a fake implementation of just the slice of each real header these adapters
actually call — same names/signatures/struct layouts, confirmed against Nordic's own nrfx source
during this project's research passes. The adapter `.c` files themselves compile completely
unchanged.

**Genuinely different simulation strategy from this project's STM32/Silicon-Labs/TI tests**: every
real transfer on this vendor goes through a registered event handler (`nrfx_uarte_event_handler_t`,
...) that's `static` inside the adapter — a test can't call it directly. Instead, the fake's
`nrfx_*_xfer`/`_tx` functions invoke whichever handler was registered for that instance
(`fake_nrf52_auto_complete`, default on) so blocking spin-waits terminate on a host machine; tests
that need to control timing set it off and fire the event manually
(`fake_nrf52_*_fire_event`). See `fakes/nrfx_uarte.h`'s header comment for a real gap this
surfaced in `../uart/hal_port_nrf52_uart.c`.

## Known limitations

- **Host x86-64, not the real target architecture** — see `../footprint/` for the one place this
  project uses a real ARM Cortex-M cross-compiler.
- **The fake is hand-written, not generated from a real header**, because no nrfx SDK is installed
  in this environment. Re-verify against your installed nrfx version if its shape has changed
  since this project's research passes — in particular, `hf_uart_init()` calling
  `nrfx_uarte_init()` without an explicit context argument (unlike this vendor's I2C/SPI
  adapters) means this test suite only exercises UARTE instance 0.
- **A busy-flag "leak" across tests is a real trap**: adapter statics are process-global and Unity
  does not restart the process between `RUN_TEST()` calls within one binary. Every busy-rejection
  test here fires the completion event before it ends to avoid leaking a stuck flag into the next
  test.
- Coverage is broad, not exhaustive — see `TEST_REPORT.md`.
