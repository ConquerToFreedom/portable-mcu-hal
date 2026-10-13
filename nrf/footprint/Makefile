# Per-adapter flash/RAM footprint for this vendor's (nRF52) adapters,
# against a REAL ARM Cortex-M4 cross-compiler (arm-none-eabi-gcc) - not
# the host gcc tests/ uses. See FOOTPRINT.md and README.md.
#
# Usage:
#   make -C nrf/footprint report   # build every .o, print a size table, write FOOTPRINT.md
#   make -C nrf/footprint clean

CC     := arm-none-eabi-gcc
CFLAGS := -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections -mthumb -mcpu=cortex-m4
ROOT   := ../..
INC    := -I$(ROOT)/include -I../tests/fakes
BUILD  := build

SRCS := \
  gpio:$(ROOT)/nrf/gpio/hal_port_nrf52.c \
  uart:$(ROOT)/nrf/uart/hal_port_nrf52_uart.c \
  i2c:$(ROOT)/nrf/i2c/hal_port_nrf52_i2c.c \
  spi:$(ROOT)/nrf/spi/hal_port_nrf52_spi.c \
  adc:$(ROOT)/nrf/adc/hal_port_nrf52_adc.c \
  timer:$(ROOT)/nrf/timer/hal_port_nrf52_timer.c \
  pwm:$(ROOT)/nrf/pwm/hal_port_nrf52_pwm.c \
  flash:$(ROOT)/nrf/flash/hal_port_nrf52_flash.c

.PHONY: all clean report

$(BUILD):
	mkdir -p $(BUILD)

all: $(BUILD)
	@for entry in $(SRCS); do \
	  name=$$(echo $$entry | cut -d: -f1); \
	  src=$$(echo $$entry | cut -d: -f2); \
	  echo "Building $$name (-mcpu=cortex-m4, -DTARGET_NRF52)"; \
	  $(CC) $(CFLAGS) -DTARGET_NRF52 $(INC) -c $$src -o $(BUILD)/$$name.o || exit 1; \
	done

clean:
	rm -rf $(BUILD) FOOTPRINT.md

report: all
	@bash generate_report.sh
