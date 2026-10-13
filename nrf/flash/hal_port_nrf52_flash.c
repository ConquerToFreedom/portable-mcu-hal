/* nRF52 (nrfx) vendor adapter for the Flash portion of hal_wrapper.h,
 * against nrfx_nvmc — confirmed directly in nrfx's own NVMC driver
 * docs: nrfx_nvmc_page_erase(address) erases one page and blocks until
 * the erase finishes (NRFX_ERROR_INVALID_ADDR if `address` isn't page-
 * aligned); nrfx_nvmc_words_write(address, src, num_words) writes
 * consecutive 32-bit words, both `address` and `src` required to be
 * word-aligned. Page size is 4096 bytes on every nRF52 part — uniform,
 * unlike STM32's F4 sector table, so hf_flash_erase_size() ignores its
 * address parameter here.
 *
 * NOT handled here: SoftDevice (BLE stack) coexistence. Nordic's own
 * guidance is that direct NVMC flash access while a SoftDevice is
 * active can stall the radio (flash operations briefly halt the CPU),
 * and the recommended path in that case is nrf_fstorage's SoftDevice
 * backend instead of nrfx_nvmc directly. This project doesn't target
 * BLE/SoftDevice builds, so this adapter uses nrfx_nvmc directly —
 * flagged here rather than silently assumed, same "document it, don't
 * hide it" treatment as every other vendor caveat in this project. If
 * you add a SoftDevice to a project using this adapter, replace this
 * file's calls with nrf_fstorage instead.
 *
 * Per hal_wrapper.h's hf_flash_t header comment: nrfx_nvmc's calls
 * used here are the normal, flash-resident blocking ones — never
 * point hf_flash_erase()/hf_flash_write() at the page(s) containing
 * your own running code or vector table.
 */
#include "hal_wrapper.h"

#if defined(TARGET_NRF52)
#if HF_ENABLE_FLASH

#include <string.h>
#include "nrfx_nvmc.h"

#define HF_NRF52_FLASH_PAGE_SIZE 4096U

bool hf_flash_init(void)
{
    return true;
}

uint32_t hf_flash_erase_size(uint32_t address)
{
    (void)address;
    return HF_NRF52_FLASH_PAGE_SIZE;
}

uint32_t hf_flash_write_align(void)
{
    return 4U;
}

bool hf_flash_erase(uint32_t address, uint32_t length)
{
    if (length == 0U ||
        (address % HF_NRF52_FLASH_PAGE_SIZE) != 0U ||
        (length % HF_NRF52_FLASH_PAGE_SIZE) != 0U) {
        return false;
    }

    uint32_t offset;
    for (offset = 0U; offset < length; offset += HF_NRF52_FLASH_PAGE_SIZE) {
        if (nrfx_nvmc_page_erase(address + offset) != NRFX_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool hf_flash_write(uint32_t address, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U ||
        (address % 4U) != 0U || (length % 4U) != 0U) {
        return false;
    }

    nrfx_nvmc_words_write(address, data, length / 4U);
    while (!nrfx_nvmc_write_done_check()) {
        /* nrfx's own docs: poll this to find out when the write
         * finished — the CPU may already be halted during the write
         * itself depending on where this code executes from, making
         * this loop a formality in that case, but it's still the
         * documented, vendor-correct way to wait either way. */
    }
    return true;
}

bool hf_flash_read(uint32_t address, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U) {
        return false;
    }
    memcpy(data, (const void *)(uintptr_t)address, length);
    return true;
}

#endif /* HF_ENABLE_FLASH */
#endif /* TARGET_NRF52 */
