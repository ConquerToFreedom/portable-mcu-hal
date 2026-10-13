# STM32 adapter unit tests

Each of this vendor's 7 adapter files has a matching `test_<peripheral>.c` here, built and run on
the host — no vendor SDK or real hardware needed. Run everything with:

```
make -C stm/tests report
```

or one binary at a time, e.g. `make -C stm/tests gpio && ./stm/tests/build/test_gpio`.

See `TEST_REPORT.md` for the last recorded full run.

## Why this approach

Each adapter `.c` file `#include`s STM32's real `main.h` (CubeMX-generated), which isn't installed
in this environment and won't be in most CI environments either. `fakes/main.h` provides a fake
implementation of just the slice of the real STM32 HAL that these adapters actually call — same
type names, function signatures, and struct field names, confirmed against ST's own STM32Cube HAL
during this project's research passes (recorded in each adapter file's own header comment). The
adapter `.c` files themselves compile completely unchanged; only what's on the include path
differs from a real CubeIDE project.

The adapters override HAL's weak callbacks (`HAL_UART_TxCpltCallback`, `HAL_I2C_ErrorCallback`,
`HAL_SPI_TxCpltCallback`, `HAL_ADC_ConvCpltCallback`, ...) directly — those are ordinary non-static
symbols, so a test can call them itself to simulate an ISR firing. Blocking calls
(`HAL_ADC_PollForConversion`, ...) just return immediately with a scriptable
`fake_stm32_next_status`. See `fakes/fake_stm32_hal.h` for the full test-control surface.

## Known limitations

- **Host x86-64, not the real target architecture** — see `../footprint/` for the one place this
  project uses a real ARM Cortex-M cross-compiler.
- **The fake is hand-written, not generated from a real header**, because no STM32Cube SDK is
  installed in this environment. Re-verify against your installed SDK if ST's HAL shape has
  changed since this project's research passes.
- **A busy-flag "leak" across tests is a real trap**: adapter statics (`s_tx_busy[]`, etc.) are
  process-global and Unity does not restart the process between `RUN_TEST()` calls within one
  binary. A test that intentionally leaves a transfer "in flight" to check busy-rejection must
  clean up (fire the completion callback) before it ends, or the *next* test in the same binary
  inherits a stuck busy flag. Every test here that does this cleans up.
- Coverage is broad, not exhaustive — see `TEST_REPORT.md`.
