# Builds and runs the unit tests for this vendor's (STM32) adapters
# against the fake HAL in tests/fakes/, using the host's own gcc - no
# vendor toolchain or real hardware involved. See tests/README.md.
#
# Usage:
#   make -C stm/tests report   # build + run everything, print a summary
#   make -C stm/tests clean

CC     ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion -g -DTARGET_STM32
ROOT   := ../..
INC    := -I$(ROOT)/include -I$(ROOT)/unity -Ifakes
BUILD  := build

UNITY := $(ROOT)/unity/unity.c
FAKE  := fakes/fake_stm32_hal.c

GPIO  := $(ROOT)/stm/gpio/hal_port_stm32.c
UART  := $(ROOT)/stm/uart/hal_port_stm32_uart.c
I2C   := $(ROOT)/stm/i2c/hal_port_stm32_i2c.c
SPI   := $(ROOT)/stm/spi/hal_port_stm32_spi.c
ADC   := $(ROOT)/stm/adc/hal_port_stm32_adc.c
TIMER := $(ROOT)/stm/timer/hal_port_stm32_timer.c
PWM   := $(ROOT)/stm/pwm/hal_port_stm32_pwm.c
FLASH := $(ROOT)/stm/flash/hal_port_stm32_flash.c

TESTS := gpio uart i2c spi adc timer pwm flash

.PHONY: all clean report $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

all: $(TESTS)

gpio: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_gpio.c $(FAKE) $(GPIO) $(UNITY) -o $(BUILD)/test_$@
uart: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_uart.c $(FAKE) $(UART) $(UNITY) -o $(BUILD)/test_$@
i2c: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_i2c.c $(FAKE) $(I2C) $(UNITY) -o $(BUILD)/test_$@
spi: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_spi.c $(FAKE) $(SPI) $(GPIO) $(UNITY) -o $(BUILD)/test_$@
adc: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_adc.c $(FAKE) $(ADC) $(UNITY) -o $(BUILD)/test_$@
timer: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_timer.c $(FAKE) $(TIMER) $(UNITY) -o $(BUILD)/test_$@
pwm: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_pwm.c $(FAKE) $(PWM) $(UNITY) -o $(BUILD)/test_$@
flash: $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_flash.c $(FAKE) $(FLASH) $(UNITY) -o $(BUILD)/test_$@

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
	  echo "ALL $(words $(TESTS)) STM32 TEST BINARIES PASSED"; \
	else \
	  echo "$$total_fail of $(words $(TESTS)) STM32 TEST BINARIES FAILED"; \
	  exit 1; \
	fi
