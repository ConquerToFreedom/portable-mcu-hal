#ifndef MCU_HAL_OS_PORT_H
#define MCU_HAL_OS_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal RTOS-shim contract. Implemented once per environment:
 * src/os_port_baremetal.c, src/os_port_freertos.c, or a customer's own
 * binding for whatever they already run. Peripheral HAL code never
 * calls into a kernel directly — only through this contract — so the
 * library never links against a specific RTOS. This is the actual
 * differentiator over Zephyr: portable AND kernel-optional.
 *
 * Signatures are a placeholder for Stage 1 review, not yet frozen. */

typedef struct os_mutex os_mutex_t; /* opaque, defined per-binding */

void os_delay_ms(uint32_t ms);
void os_mutex_lock(os_mutex_t *m);
void os_mutex_unlock(os_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* MCU_HAL_OS_PORT_H */
