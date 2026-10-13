# GPIO vendor API reference

Reference signatures for writing each `<vendor>/gpio/hal_port_<vendor>.c`
adapter against `include/hal_wrapper.h`. Pulled from current vendor docs — verify
against the SDK version actually installed before relying on an exact
signature, since vendor APIs do shift between SDK releases.

Vendor/SDK generation covered by each adapter — see README.md's "SDK
versions this wrapper targets" for the full explanation of why nRF is
one adapter and Silabs is two:

| Adapter file | Target macro | SDK generation |
|---|---|---|
| `stm/gpio/hal_port_stm32.c` | `TARGET_STM32` | STM32 HAL, any current STM32Cube package |
| `nrf/gpio/hal_port_nrf52.c` | `TARGET_NRF52` | nrfx `nrf_gpio.h` — same file in legacy nRF5 SDK and current nRF Connect SDK |
| `silicon_labs/gpio/hal_port_silabs.c` | `TARGET_SILABS` | Classic emlib — still supported, no Series 3 |
| `silicon_labs/gpio/hal_port_silabs_slgpio.c` | `TARGET_SILABS_SLGPIO` | Current `sl_gpio` driver — Series 2 + Series 3, production-quality as of the 2025 SDK releases |
| `ti/gpio/hal_port_ti_mspm0.c` | `TARGET_TI_MSPM0` | driverlib, any current MSPM0-SDK package |

| | STM32 HAL | nRF52 (nrfx) | Silicon Labs (emlib) | Silicon Labs (`sl_gpio`) | TI MSPM0 (driverlib) |
|---|---|---|---|---|---|
| Pin identity | `GPIO_TypeDef *port` + `uint16_t pin` (bitmask) | single `uint32_t pin_number` via `NRF_GPIO_PIN_MAP(port,pin)` | `GPIO_Port_TypeDef port` + `unsigned int pin` (index) | `sl_gpio_t{port, pin}` struct, passed by pointer | `GPIO_Regs *gpio` (port) + `uint32_t pins` (bitmask) |
| Init/mode | `HAL_GPIO_Init(port, &GPIO_InitTypeDef)` | `nrf_gpio_cfg_output(pin)` / `nrf_gpio_cfg_input(pin, pull)` | `GPIO_PinModeSet(port, pin, mode, out)` | `sl_gpio_set_pin_mode(&gpio, mode, out)` → `sl_status_t` | Direction only: `DL_GPIO_enableOutput(gpio, pins)` / `DL_GPIO_disableOutput(gpio, pins)`. Pull/open-drain need a separate IOMUX `PINCM` index our scalar can't derive — `hf_gpio_init()` returns `false` for those three modes on this vendor. See the pull/open-drain asymmetry table in `docs/architecture.md`. |
| Write | `HAL_GPIO_WritePin(port, pin, SET/RESET)` | `nrf_gpio_pin_set(pin)` / `nrf_gpio_pin_clear(pin)` | `GPIO_PinOutSet(port, pin)` / `GPIO_PinOutClear(port, pin)` | `sl_gpio_set_pin(&gpio)` / `sl_gpio_clear_pin(&gpio)` | `DL_GPIO_setPins(gpio, pins)` / `DL_GPIO_clearPins(gpio, pins)` |
| Read | `HAL_GPIO_ReadPin(port, pin)` → `GPIO_PinState` | `nrf_gpio_pin_read(pin)` → `uint32_t` | `GPIO_PinInGet(port, pin)` → `unsigned int` (reads DIN — actual pad state; `GPIO_PinOutGet` reads DOUT, the last-driven value, and is the wrong function here) | `sl_gpio_get_pin_input(&gpio, &value)` → `sl_status_t`, value out-param (same DIN-not-DOUT distinction as emlib) | `DL_GPIO_readPins(gpio, pins)` → `uint32_t` |
| Native toggle | Yes — `HAL_GPIO_TogglePin(port, pin)` | No — confirmed no atomic toggle register; `nrf_gpio_pin_toggle(pin)` does a read-modify-write of `OUT` into `OUTSET`/`OUTCLR`. Adapter calls that SDK function directly. | Yes — `GPIO_PinOutToggle(port, pin)` | Yes — `sl_gpio_toggle_pin(&gpio)` | Yes — `DL_GPIO_togglePins(gpio, pins)`, documented as pure hardware op |
| Identity shape | Bitmask | Index | Index | Struct (port+index) | Bitmask |

## Silicon Labs: emlib vs sl_gpio mode enum mapping

Both adapters implement the same five `hf_gpio_mode_t` values, but the
underlying enum member names differ completely — verified against
`platform/service/device_manager/inc/sl_device_gpio.h` in
`SiliconLabs/simplicity_sdk` (`sisdk-2025.6`) before writing the
`sl_gpio` adapter:

| `hf_gpio_mode_t` | emlib `GPIO_Mode_TypeDef` | `sl_gpio` `sl_gpio_mode_t` |
|---|---|---|
| `HF_GPIO_INPUT` | `gpioModeInput` | `SL_GPIO_MODE_INPUT` |
| `HF_GPIO_INPUT_PULLUP` | `gpioModeInputPull`, out=1 | `SL_GPIO_MODE_INPUT_PULL`, out=true |
| `HF_GPIO_INPUT_PULLDOWN` | `gpioModeInputPull`, out=0 | `SL_GPIO_MODE_INPUT_PULL`, out=false |
| `HF_GPIO_OUTPUT_PUSHPULL` | `gpioModePushPull` | `SL_GPIO_MODE_PUSH_PULL` |
| `HF_GPIO_OUTPUT_OPENDRAIN` | `gpioModeWiredAnd` | `SL_GPIO_MODE_WIRED_AND` |

Pull direction is an `out` argument on both APIs, not a separate mode
value — easy to miss if you're used to a vendor that has distinct
pull-up/pull-down enum values (STM32, nRF52 both do).

## Adapter decode pattern

Every adapter receives the same `hf_gpio_t` scalar and decodes it the
same way:

```c
#define HF_PIN_PORT(p)   ((p) >> 5)
#define HF_PIN_NUM(p)    ((p) & 0x1F)
```

- STM32 / TI (bitmask vendors): adapter does `1U << HF_PIN_NUM(p)` before
  calling the vendor function, and looks up the port base from a small
  `const` array indexed by `HF_PIN_PORT(p)`.
- nRF (index vendor): `HF_PIN_PORT(p)`/`HF_PIN_NUM(p)` aren't even
  decoded separately — `hf_gpio_t`'s encoding matches Nordic's own
  `NRF_GPIO_PIN_MAP` exactly, so the whole scalar passes straight
  through as the nrfx pin number.
- Silabs emlib (index vendor): adapter passes `HF_PIN_PORT(p)` and
  `HF_PIN_NUM(p)` as two separate function arguments (`GPIO_Port_TypeDef`
  values already match `HF_PIN_PORT(p)` directly, no lookup table).
- Silabs `sl_gpio` (index vendor, struct-based): same port/index values
  as emlib, but packed into a local `sl_gpio_t{port, pin}` struct and
  passed by pointer instead of as two loose arguments.

## Toggle fallback (only where native toggle isn't available)

```c
void hf_gpio_toggle(hf_gpio_t pin) {
    hf_gpio_write(pin, !hf_gpio_read(pin));  // fallback only
}
```
Use this only on vendors confirmed to lack a hardware toggle register.
Do not apply it universally — it silently hides a real cost difference on
vendors that do have native toggle.
