# Per-adapter flash/RAM footprint for this vendor's (Silicon Labs)
# adapters, against a REAL ARM cross-compiler (arm-none-eabi-gcc) - not
# the host gcc tests/ uses. Covers BOTH target macros this vendor
# ships, same as the adapter files themselves: TARGET_SILABS (classic
# emlib, Cortex-M4) and TARGET_SILABS_SLGPIO (current sl_gpio/sl_i2c/
# IADC, Cortex-M33 - Series 2/3 parts use the M33 core). See
# FOOTPRINT.md and README.md.
#
# Usage:
#   make -C silicon_labs/footprint report   # build every .o, print a size table, write FOOTPRINT.md
#   make -C silicon_labs/footprint clean

CC     := arm-none-eabi-gcc
CFLAGS := -std=c99 -Os -Wall -Wextra -ffunction-sections -fdata-sections -mthumb
ROOT   := ../..
INC    := -I$(ROOT)/include
BUILD  := build

CLASSIC_INC := ../tests/fakes/silabs
SLGPIO_INC  := ../tests/fakes/silabs_slgpio,../tests/fakes/silabs

# name:cpu:target-macro:include-dirs(comma-separated, no spaces):source-file
SRCS := \
  gpio:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/gpio/hal_port_silabs.c \
  uart:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/uart/hal_port_silabs_uart.c \
  i2c:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/i2c/hal_port_silabs_i2c.c \
  spi:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/spi/hal_port_silabs_spi.c \
  adc:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/adc/hal_port_silabs_adc.c \
  timer:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/timer/hal_port_silabs_timer.c \
  pwm:cortex-m4:TARGET_SILABS:$(CLASSIC_INC):$(ROOT)/silicon_labs/pwm/hal_port_silabs_pwm.c \
  slgpio_gpio:cortex-m33:TARGET_SILABS_SLGPIO:$(SLGPIO_INC):$(ROOT)/silicon_labs/gpio/hal_port_silabs_slgpio.c \
  slgpio_uart:cortex-m33:TARGET_SILABS_SLGPIO:$(CLASSIC_INC):$(ROOT)/silicon_labs/uart/hal_port_silabs_slgpio_uart.c \
  slgpio_i2c:cortex-m33:TARGET_SILABS_SLGPIO:$(SLGPIO_INC):$(ROOT)/silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c \
  slgpio_spi:cortex-m33:TARGET_SILABS_SLGPIO:$(CLASSIC_INC):$(ROOT)/silicon_labs/spi/hal_port_silabs_slgpio_spi.c \
  slgpio_adc:cortex-m33:TARGET_SILABS_SLGPIO:$(SLGPIO_INC):$(ROOT)/silicon_labs/adc/hal_port_silabs_slgpio_adc.c \
  slgpio_timer:cortex-m33:TARGET_SILABS_SLGPIO:$(SLGPIO_INC):$(ROOT)/silicon_labs/timer/hal_port_silabs_slgpio_timer.c \
  slgpio_pwm:cortex-m33:TARGET_SILABS_SLGPIO:$(SLGPIO_INC):$(ROOT)/silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c

.PHONY: all clean report

$(BUILD):
	mkdir -p $(BUILD)

all: $(BUILD)
	@for entry in $(SRCS); do \
	  name=$$(echo $$entry | cut -d: -f1); \
	  cpu=$$(echo $$entry | cut -d: -f2); \
	  macro=$$(echo $$entry | cut -d: -f3); \
	  incdirs=$$(echo $$entry | cut -d: -f4); \
	  src=$$(echo $$entry | cut -d: -f5); \
	  incflags=""; \
	  for d in $$(echo $$incdirs | tr ',' ' '); do incflags="$$incflags -I$$d"; done; \
	  echo "Building $$name (-mcpu=$$cpu, -D$$macro)"; \
	  $(CC) $(CFLAGS) -mcpu=$$cpu -D$$macro $(INC) $$incflags -c $$src -o $(BUILD)/$$name.o || exit 1; \
	done

clean:
	rm -rf $(BUILD) FOOTPRINT.md

report: all
	@bash generate_report.sh
