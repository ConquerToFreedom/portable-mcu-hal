# Per-adapter flash/RAM footprint for this vendor's (STM32) adapters,
# against a REAL ARM Cortex-M4 cross-compiler (arm-none-eabi-gcc) - not
# the host gcc tests/ uses. See FOOTPRINT.md and README.md.
#
# Each adapter .c file is compiled to a standalone .o (never linked
# into a full firmware image - no real vendor SDK is installed here)
# against the same fake STM32 HAL header tests/fakes/ uses, since
# compiling only needs declarations, not the fake's behavioral body.
# -Os matches what a size-conscious embedded project actually builds
# with.
#
# Usage:
#   make -C stm/footprint report   # build every .o, print a size table, write FOOTPRINT.md
#   make -C stm/footprint clean

CC     := arm-none-eabi-gcc
CFLAGS := -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections -mthumb -mcpu=cortex-m4
ROOT   := ../..
INC    := -I$(ROOT)/include -I../tests/fakes
BUILD  := build

SRCS := \
  gpio:$(ROOT)/stm/gpio/hal_port_stm32.c \
  uart:$(ROOT)/stm/uart/hal_port_stm32_uart.c \
  i2c:$(ROOT)/stm/i2c/hal_port_stm32_i2c.c \
  spi:$(ROOT)/stm/spi/hal_port_stm32_spi.c \
  adc:$(ROOT)/stm/adc/hal_port_stm32_adc.c \
  timer:$(ROOT)/stm/timer/hal_port_stm32_timer.c \
  pwm:$(ROOT)/stm/pwm/hal_port_stm32_pwm.c

.PHONY: all clean report

$(BUILD):
	mkdir -p $(BUILD)

all: $(BUILD)
	@for entry in $(SRCS); do \
	  name=$$(echo $$entry | cut -d: -f1); \
	  src=$$(echo $$entry | cut -d: -f2); \
	  echo "Building $$name (-mcpu=cortex-m4, -DTARGET_STM32)"; \
	  $(CC) $(CFLAGS) -DTARGET_STM32 $(INC) -c $$src -o $(BUILD)/$$name.o || exit 1; \
	done

clean:
	rm -rf $(BUILD) FOOTPRINT.md

report: all
	@bash generate_report.sh
