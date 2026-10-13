# Builds and runs the unit tests for this vendor's (Silicon Labs)
# adapters against the fake emlib/sl_gpio/sl_i2c/IADC in fakes/, using
# the host's own gcc - no vendor toolchain or real hardware involved.
# Covers BOTH target macros this vendor ships, same as the adapter
# files themselves: TARGET_SILABS (classic emlib) and
# TARGET_SILABS_SLGPIO (current sl_gpio/sl_i2c/IADC, UART/SPI shared
# unchanged with classic - see tests/README.md).
#
# Usage:
#   make -C silicon_labs/tests report   # build + run everything, print a summary
#   make -C silicon_labs/tests clean

CC     ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion -g
ROOT   := ../..
INC    := -I$(ROOT)/include -I$(ROOT)/unity
BUILD  := build

UNITY       := $(ROOT)/unity/unity.c
CLASSIC_INC := -Ifakes/silabs
SLGPIO_INC  := -Ifakes/silabs_slgpio -Ifakes/silabs
CLASSIC_FAKE := fakes/silabs/fake_silabs.c
SLGPIO_FAKE  := fakes/silabs_slgpio/fake_silabs_slgpio.c

GPIO        := $(ROOT)/silicon_labs/gpio/hal_port_silabs.c
UART        := $(ROOT)/silicon_labs/uart/hal_port_silabs_uart.c
I2C         := $(ROOT)/silicon_labs/i2c/hal_port_silabs_i2c.c
SPI         := $(ROOT)/silicon_labs/spi/hal_port_silabs_spi.c
ADC         := $(ROOT)/silicon_labs/adc/hal_port_silabs_adc.c
TIMER       := $(ROOT)/silicon_labs/timer/hal_port_silabs_timer.c
PWM         := $(ROOT)/silicon_labs/pwm/hal_port_silabs_pwm.c
FLASH       := $(ROOT)/silicon_labs/flash/hal_port_silabs_flash.c

SLGPIO_GPIO  := $(ROOT)/silicon_labs/gpio/hal_port_silabs_slgpio.c
SLGPIO_UART  := $(ROOT)/silicon_labs/uart/hal_port_silabs_slgpio_uart.c
SLGPIO_I2C   := $(ROOT)/silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c
SLGPIO_SPI   := $(ROOT)/silicon_labs/spi/hal_port_silabs_slgpio_spi.c
SLGPIO_ADC   := $(ROOT)/silicon_labs/adc/hal_port_silabs_slgpio_adc.c
SLGPIO_TIMER := $(ROOT)/silicon_labs/timer/hal_port_silabs_slgpio_timer.c
SLGPIO_PWM   := $(ROOT)/silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c
SLGPIO_FLASH := $(ROOT)/silicon_labs/flash/hal_port_silabs_slgpio_flash.c

TESTS := silabs_gpio silabs_uart silabs_i2c silabs_spi silabs_adc silabs_timer silabs_pwm silabs_flash \
         silabs_slgpio_gpio silabs_slgpio_uart silabs_slgpio_i2c silabs_slgpio_spi silabs_slgpio_adc \
         silabs_slgpio_timer silabs_slgpio_pwm silabs_slgpio_flash

.PHONY: all clean report $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

all: $(TESTS)

# --- classic (TARGET_SILABS) ---
silabs_gpio: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_gpio.c $(CLASSIC_FAKE) $(GPIO) $(UNITY) -o $(BUILD)/test_$@
silabs_uart: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_uart.c $(CLASSIC_FAKE) $(UART) $(UNITY) -o $(BUILD)/test_$@
silabs_i2c: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_i2c.c $(CLASSIC_FAKE) $(I2C) $(UNITY) -o $(BUILD)/test_$@
silabs_spi: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_spi.c $(CLASSIC_FAKE) $(SPI) $(GPIO) $(UNITY) -o $(BUILD)/test_$@
silabs_adc: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_adc.c $(CLASSIC_FAKE) $(ADC) $(UNITY) -o $(BUILD)/test_$@
silabs_timer: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_timer.c $(CLASSIC_FAKE) $(TIMER) $(UNITY) -o $(BUILD)/test_$@
silabs_pwm: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_pwm.c $(CLASSIC_FAKE) $(PWM) $(UNITY) -o $(BUILD)/test_$@
silabs_flash: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS $(INC) $(CLASSIC_INC) test_silabs_flash.c $(CLASSIC_FAKE) $(FLASH) $(UNITY) -o $(BUILD)/test_$@

# --- current (TARGET_SILABS_SLGPIO) ---
silabs_slgpio_gpio: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_gpio.c $(SLGPIO_FAKE) $(SLGPIO_GPIO) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_uart: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(CLASSIC_INC) test_silabs_slgpio_uart.c $(CLASSIC_FAKE) $(SLGPIO_UART) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_i2c: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_i2c.c $(SLGPIO_FAKE) $(CLASSIC_FAKE) $(SLGPIO_I2C) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_spi: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_spi.c $(CLASSIC_FAKE) $(SLGPIO_FAKE) $(SLGPIO_SPI) $(SLGPIO_GPIO) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_adc: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_adc.c $(SLGPIO_FAKE) $(CLASSIC_FAKE) $(SLGPIO_ADC) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_timer: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_timer.c $(SLGPIO_FAKE) $(CLASSIC_FAKE) $(SLGPIO_TIMER) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_pwm: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(SLGPIO_INC) test_silabs_slgpio_pwm.c $(SLGPIO_FAKE) $(CLASSIC_FAKE) $(SLGPIO_PWM) $(UNITY) -o $(BUILD)/test_$@
silabs_slgpio_flash: $(BUILD)
	$(CC) $(CFLAGS) -DTARGET_SILABS_SLGPIO $(INC) $(CLASSIC_INC) test_silabs_slgpio_flash.c $(CLASSIC_FAKE) $(SLGPIO_FLASH) $(UNITY) -o $(BUILD)/test_$@

clean:
	rm -rf $(BUILD)

report: all
	@total_fail=0; \
	for t in $(TESTS); do \
	  echo "=== $$t ==="; \
	  if ! $(BUILD)/test_$$t; then total_fail=$$((total_fail + 1)); fi; \
	  echo; \
	done; \
	echo "==========================================="; \
	if [ $$total_fail -eq 0 ]; then \
	  echo "ALL $(words $(TESTS)) SILICON LABS TEST BINARIES PASSED"; \
	else \
	  echo "$$total_fail of $(words $(TESTS)) SILICON LABS TEST BINARIES FAILED"; \
	  exit 1; \
	fi
