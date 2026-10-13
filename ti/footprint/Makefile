# Per-adapter flash/RAM footprint for this vendor's (TI MSPM0) adapters,
# against a REAL ARM Cortex-M0+ cross-compiler (arm-none-eabi-gcc) - not
# the host gcc tests/ uses. See FOOTPRINT.md and README.md.
#
# Usage:
#   make -C ti/footprint report   # build every .o, print a size table, write FOOTPRINT.md
#   make -C ti/footprint clean

CC     := arm-none-eabi-gcc
CFLAGS := -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections -mthumb -mcpu=cortex-m0plus
ROOT   := ../..
INC    := -I$(ROOT)/include -I../tests/fakes
BUILD  := build

SRCS := \
  gpio:$(ROOT)/ti/gpio/hal_port_ti_mspm0.c \
  uart:$(ROOT)/ti/uart/hal_port_ti_mspm0_uart.c \
  i2c:$(ROOT)/ti/i2c/hal_port_ti_mspm0_i2c.c \
  spi:$(ROOT)/ti/spi/hal_port_ti_mspm0_spi.c \
  adc:$(ROOT)/ti/adc/hal_port_ti_mspm0_adc.c \
  timer:$(ROOT)/ti/timer/hal_port_ti_mspm0_timer.c \
  pwm:$(ROOT)/ti/pwm/hal_port_ti_mspm0_pwm.c

.PHONY: all clean report

$(BUILD):
	mkdir -p $(BUILD)

all: $(BUILD)
	@for entry in $(SRCS); do \
	  name=$$(echo $$entry | cut -d: -f1); \
	  src=$$(echo $$entry | cut -d: -f2); \
	  echo "Building $$name (-mcpu=cortex-m0plus, -DTARGET_TI_MSPM0)"; \
	  $(CC) $(CFLAGS) -DTARGET_TI_MSPM0 $(INC) -c $$src -o $(BUILD)/$$name.o || exit 1; \
	done

clean:
	rm -rf $(BUILD) FOOTPRINT.md

report: all
	@bash generate_report.sh
