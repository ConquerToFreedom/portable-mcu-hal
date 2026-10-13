# STM32 adapter footprint measurement

Measures how much flash (`.text`) and RAM (`.data`+`.bss`) each of this vendor's 7 adapter files
costs on real Cortex-M4, using a real ARM cross-compiler (`arm-none-eabi-gcc`) — not the host
`gcc` `../tests/` uses. See `FOOTPRINT.md` for the last recorded numbers and its "What these
numbers do and don't mean" section before quoting one out of context.

## Running it

Needs `arm-none-eabi-gcc`/`arm-none-eabi-size`/`arm-none-eabi-nm` on `PATH`:

```sh
# Debian/Ubuntu, if your apt mirror is fast:
sudo apt-get install gcc-arm-none-eabi

# Otherwise, a prebuilt toolchain from xPack (fast even when apt is throttled):
curl -LO https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v13.3.1-1.1/xpack-arm-none-eabi-gcc-13.3.1-1.1-linux-x64.tar.gz
mkdir -p /opt/arm-toolchain && tar -xzf xpack-arm-none-eabi-gcc-*-linux-x64.tar.gz -C /opt/arm-toolchain --strip-components=1
export PATH="/opt/arm-toolchain/bin:$PATH"
```

Then:

```sh
make -C stm/footprint report   # builds every adapter .o, writes stm/footprint/FOOTPRINT.md
make -C stm/footprint clean
```

## How it works

Each adapter `.c` file compiles standalone to a `.o` with `-mcpu=cortex-m4 -Os`, against the same
fake STM32 HAL header `../tests/fakes/` uses for unit testing (declarations only — an object-file
compile never needs the fake's behavioral body). No real vendor SDK is installed here, so nothing
is linked into a full firmware image — this measures the adapter's own code in isolation, the
right scope for this project's contract.
