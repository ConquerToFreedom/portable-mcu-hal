/* Fake stand-in for nrfx's nrfx_nvmc.h, used ONLY to host-compile and
 * unit-test this vendor's Flash adapter under nrf/flash/. Not a copy
 * of Nordic's header.
 */
#ifndef HAL_WRAPPER_FAKE_NRFX_NVMC_H
#define HAL_WRAPPER_FAKE_NRFX_NVMC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
#define HAL_WRAPPER_FAKE_NRFX_ERR_DEFINED
typedef enum { NRFX_SUCCESS = 0, NRFX_ERROR_INTERNAL = 1 } nrfx_err_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

nrfx_err_t nrfx_nvmc_page_erase(uint32_t address);
void nrfx_nvmc_words_write(uint32_t address, void const *src, uint32_t num_words);
bool nrfx_nvmc_write_done_check(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WRAPPER_FAKE_NRFX_NVMC_H */
