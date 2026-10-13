# Builds and runs the unit tests for this vendor's (TI MSPM0) adapters
# against the fake driverlib in tests/fakes/, using the host's own gcc -
# no vendor toolchain or real hardware involved. See tests/README.md.
#
# Usage:
#   make -C ti/tests report   # build + run everything, print a summary
#   make -C ti/tests clean

CC     ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wsign-conversion -g -DTARGET_TI_MSPM0
ROOT   := ../..
INC    := -I$(ROOT)/include -I$(ROOT)/unity -Ifakes
BUILD  := build

UNITY := $(ROOT)/unity/unity.c
FAKE  := fakes/fake_ti_mspm0.c

GPIO  := $(ROOT)/ti/gpio/hal_port_ti_mspm0.c
UART  := $(ROOT)/ti/uart/hal_port_ti_mspm0_uart.c
I2C   := $(ROOT)/ti/i2c/hal_port_ti_mspm0_i2c.c
SPI   := $(ROOT)/ti/spi/hal_port_ti_mspm0_spi.c
ADC   := $(ROOT)/ti/adc/hal_port_ti_mspm0_adc.c
TIMER := $(ROOT)/ti/timer/hal_port_ti_mspm0_timer.c
PWM   := $(ROOT)/ti/pwm/hal_port_ti_mspm0_pwm.c

TESTS := gpio uart i2c spi adc timer pwm

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
	  echo "ALL $(words $(TESTS)) TI MSPM0 TEST BINARIES PASSED"; \
	else \
	  echo "$$total_fail of $(words $(TESTS)) TI MSPM0 TEST BINARIES FAILED"; \
	  exit 1; \
	fi
