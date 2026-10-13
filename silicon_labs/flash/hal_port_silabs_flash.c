/* Silicon Labs classic (emlib) vendor adapter for the Flash portion of
 * hal_wrapper.h, against MSC (Memory System Controller) — em_msc.h.
 * Confirmed directly against Silicon Labs' own emlib MSC docs: MSC_Init()
 * must be called before any flash write/erase (this adapter's
 * hf_flash_init() does so — the same category of "a real per-vendor
 * requirement, not invented busywork" this project's GPIO adapter fix
 * already found missing once for peripheral clock-enables, see
 * docs/architecture.md's Silicon Labs clock-enable bug-fix section;
 * MSC_Init() is this peripheral's equivalent and this adapter does not
 * skip it). MSC_ErasePage(uint32_t *startAddress) erases one page;
 * MSC_WriteWord(uint32_t *address, const void *data, uint32_t numBytes)
 * requires a word-aligned address and a numBytes divisible by 4 — both
 * confirmed in the docs' own constraint notes.
 *
 * Return-type enum member names (msc_Return_TypeDef's mscReturnOk and
 * friends) were NOT independently re-confirmed against your installed
 * Gecko SDK version's em_msc.h in this fetch pass — cross-check before
 * relying on this in production, same flagged-uncertainty treatment
 * already given to this vendor's other adapters' part-specific
 * constants (e.g. the TIMER-count caveat).
 *
 * Flash page size is NOT uniform across Silicon Labs parts (unlike
 * nRF52/TI) — it depends on total flash size (2 KB on many smaller
 * EFM32/EFR32 Series 0/1 parts, up to 8 KB on some larger Series 2
 * parts). HF_SILABS_FLASH_PAGE_SIZE below is a placeholder for the
 * common small/mid-range case — EDIT it to match your part's
 * reference manual before relying on this adapter, same "trim the
 * table to match your part" precedent as this vendor's TIMER-count
 * caveat.
 *
 * Per hal_wrapper.h's hf_flash_t header comment: MSC's write functions
 * are themselves placed in RAM by the SDK's build system on affected
 * parts (SL_RAMFUNC) specifically so they can safely operate on the
 * bank code executes from — this adapter relies on that SDK behavior
 * rather than re-implementing it, but the same rule still applies in
 * spirit: avoid pointing hf_flash_erase()/hf_flash_write() at a page
 * your own const data depends on remaining stable mid-operation.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS)
#if HF_ENABLE_FLASH

#include <string.h>
#include "em_msc.h"

#define HF_SILABS_FLASH_PAGE_SIZE 2048U /* EDIT for your part — see file header */

bool hf_flash_init(void)
{
    MSC_Init();
    return true;
}

uint32_t hf_flash_erase_size(uint32_t address)
{
    (void)address;
    return HF_SILABS_FLASH_PAGE_SIZE;
}

uint32_t hf_flash_write_align(void)
{
    return 4U;
}

bool hf_flash_erase(uint32_t address, uint32_t length)
{
    if (length == 0U ||
        (address % HF_SILABS_FLASH_PAGE_SIZE) != 0U ||
        (length % HF_SILABS_FLASH_PAGE_SIZE) != 0U) {
        return false;
    }

    uint32_t offset;
    for (offset = 0U; offset < length; offset += HF_SILABS_FLASH_PAGE_SIZE) {
        if (MSC_ErasePage((uint32_t *)(uintptr_t)(address + offset)) != mscReturnOk) {
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

    return MSC_WriteWord((uint32_t *)(uintptr_t)address, data, length) == mscReturnOk;
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
#endif /* TARGET_SILABS */
