# Builds and runs the unit tests for this vendor's (nRF52) adapters
# against the fake nrfx in tests/fakes/, using the host's own gcc - no
# vendor toolchain or real hardware involved. See tests/README.md.
#
# Usage:
#   make -C nrf/tests report   # build + run everything, print a summary
#   make -C nrf/tests clean

CC     ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion -g -DTARGET_NRF52
ROOT   := ../..
INC    := -I$(ROOT)/include -I$(ROOT)/unity -Ifakes
BUILD  := build

UNITY := $(ROOT)/unity/unity.c
FAKE  := fakes/fake_nrf52.c

GPIO  := $(ROOT)/nrf/gpio/hal_port_nrf52.c
UART  := $(ROOT)/nrf/uart/hal_port_nrf52_uart.c
I2C   := $(ROOT)/nrf/i2c/hal_port_nrf52_i2c.c
SPI   := $(ROOT)/nrf/spi/hal_port_nrf52_spi.c
ADC   := $(ROOT)/nrf/adc/hal_port_nrf52_adc.c
TIMER := $(ROOT)/nrf/timer/hal_port_nrf52_timer.c
PWM   := $(ROOT)/nrf/pwm/hal_port_nrf52_pwm.c
FLASH := $(ROOT)/nrf/flash/hal_port_nrf52_flash.c

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
	  echo "ALL $(words $(TESTS)) NRF52 TEST BINARIES PASSED"; \
	else \
	  echo "$$total_fail of $(words $(TESTS)) NRF52 TEST BINARIES FAILED"; \
	  exit 1; \
	fi
