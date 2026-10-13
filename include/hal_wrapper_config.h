#ifndef MCU_HAL_WRAPPER_CONFIG_H
#define MCU_HAL_WRAPPER_CONFIG_H

/* Edit this file to enable/disable peripherals you're not using, so
 * unused adapter code doesn't add to flash on a resource-constrained
 * part. Defaults: all eight peripherals enabled — every one has vendor
 * adapters written across all five target macros. On classic Silicon
 * Labs targets (TARGET_SILABS / TARGET_SILABS_SLGPIO), UART and SPI
 * share the same USART hardware — see those adapters' file headers
 * before enabling both for the same instance. On classic Silicon Labs
 * (TARGET_SILABS) specifically, Timer and PWM also share the same
 * TIMER peripheral family — Timer's adapter claims TIMER0/TIMER1, PWM's
 * claims TIMER2/TIMER3; verify your part has enough TIMER blocks before
 * enabling both, same "loud, not silent" precedent as every other
 * instance-table trim in this project. Flash has its own, bigger
 * caveat that isn't a simple enable/disable trim — see hf_flash_t's
 * header comment in hal_wrapper.h before enabling it: never erase or
 * write the region your own code/vector-table/const-data lives in. */

#define HF_ENABLE_GPIO   1
#define HF_ENABLE_UART   1
#define HF_ENABLE_I2C    1
#define HF_ENABLE_SPI    1
#define HF_ENABLE_ADC    1
#define HF_ENABLE_TIMER  1
#define HF_ENABLE_PWM    1
#define HF_ENABLE_FLASH  1

#endif /* MCU_HAL_WRAPPER_CONFIG_H */
