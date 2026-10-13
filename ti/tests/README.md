# TI MSPM0 adapter unit tests

Each of this vendor's 7 adapter files has a matching `test_<peripheral>.c` here, built and run on
the host — no vendor SDK or real hardware needed. Run everything with:

```
make -C ti/tests report
```

or one binary at a time, e.g. `make -C ti/tests gpio && ./ti/tests/build/test_gpio`.

See `TEST_REPORT.md` for the last recorded full run.

## Why this approach

Each adapter `.c` file `#include`s a SysConfig-generated `ti_msp_dl_config.h`, which isn't
installed in this environment. `fakes/ti_msp_dl_config.h` provides a fake implementation of just
the slice of TI driverlib these adapters actually call — same names/signatures/struct layouts,
confirmed against TI's official `TexasInstruments/mspm0-sdk` GitHub mirror during this project's
research passes. The adapter `.c` files themselves compile completely unchanged.

Adapters define their own non-static `UARTx_IRQHandler`/`I2Cx_IRQHandler`/`SPIx_IRQHandler`/
`ADCx_IRQHandler` functions — callable directly by a test to simulate an interrupt firing, same
pattern as this project's classic-Silicon-Labs fakes. One quirk worth noting:
`DL_ADC12_getPendingInterrupt()`'s return value is used both as a bitmask (`&`) and by equality
(`==`) in the same adapter file — the fake models it as a `#define` bitmask constant, not an enum,
so both usages agree, matching real driverlib's IIDX register convention.

## Known limitations

- **Host x86-64, not the real target architecture** — see `../footprint/` for the one place this
  project uses a real ARM Cortex-M cross-compiler.
- **The fake is hand-written, not generated from a real header**, because no MSPM0-SDK is
  installed in this environment. Several UART/I2C/SPI/ADC constant names in the real adapters
  themselves are flagged as not independently confirmed (see each adapter's own header comment) —
  the fake follows the same assumptions, so a fake test pass here doesn't confirm those names are
  actually correct against your installed SDK.
- **A busy-flag "leak" across tests is a real trap**: adapter statics are process-global and Unity
  does not restart the process between `RUN_TEST()` calls within one binary. Every busy-rejection
  test here fires the completion interrupt before it ends to avoid leaking a stuck flag into the
  next test.
- Coverage is broad, not exhaustive — see `TEST_REPORT.md`.
