/* Fake stand-in for Silicon Labs' sl_status.h, used ONLY to
 * host-compile and unit-test this vendor's current-driver
 * (TARGET_SILABS_SLGPIO) adapters. Not a copy of Silicon Labs' header.
 */
#ifndef HAL_WRAPPER_FAKE_SL_STATUS_H
#define HAL_WRAPPER_FAKE_SL_STATUS_H

#include <stdint.h>

typedef uint32_t sl_status_t;
#define SL_STATUS_OK 0x0000U
#define SL_STATUS_FAIL 0x0001U

#endif /* HAL_WRAPPER_FAKE_SL_STATUS_H */
