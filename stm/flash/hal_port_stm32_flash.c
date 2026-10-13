/* STM32 HAL vendor adapter for the Flash portion of hal_wrapper.h.
 *
 * Reference family is F4 (same reference point this vendor's ADC/Timer
 * adapters already use): FLASH_TYPEPROGRAM_WORD (32-bit programming)
 * and FLASH_TYPEERASE_SECTORS with FLASH_VOLTAGE_RANGE_3. Other STM32
 * families need different constants here — L4/G4/G0 require
 * FLASH_TYPEPROGRAM_DOUBLEWORD (64-bit), H7 requires its 256-bit flash
 * word and FLASH_TYPEERASE_SECTORS without a voltage range parameter;
 * substitute them if your part isn't an F4, same per-family
 * substitution already flagged throughout this vendor's other
 * adapters.
 *
 * Sector table below (s_sector_table) is the F407/F429-class 1 MB
 * single-bank memory map confirmed in RM0090: four 16 KB sectors, one
 * 64 KB sector, then seven 128 KB sectors (sectors 0-11, 0x08000000 to
 * 0x080FFFFF). This is the single biggest "varies by part" surface
 * this project has hit — unlike every other vendor's flash, F4's
 * sector sizes are NOT uniform, so hf_flash_erase_size() must look an
 * address up in this table rather than return one constant. EDIT this
 * table (or extend it with bank-2 sectors 12-23) to match your exact
 * part's reference manual memory map before relying on this adapter —
 * a wrong table silently erases the wrong byte range.
 *
 * hf_flash_write() takes `uint8_t *data` (non-const, matching
 * hal_wrapper.h's declaration) because HAL_FLASH_Program's own
 * signature takes the word value by uint64_t, not a pointer — this
 * adapter reads *through* the caller's pointer, never writes through
 * it; the header's non-const parameter is for symmetry with other
 * vendors' adapters that do pass the pointer into a vendor call
 * expecting non-const, not a sign this one mutates the caller's
 * buffer.
 *
 * Per this file's own top-of-header-comment-block rule (see
 * hal_wrapper.h's hf_flash_t section): this adapter calls
 * HAL_FLASH_Program/HAL_FLASHEx_Erase directly, the normal
 * flash-resident blocking calls, NOT a RAM-executed variant. Never
 * point hf_flash_erase()/hf_flash_write() at the sector(s) containing
 * your own running code or vector table.
 */
#include "hal_wrapper.h"

#if defined(TARGET_STM32)
#if HF_ENABLE_FLASH

#include <stddef.h>
#include <string.h>
#include "main.h"

typedef struct {
    uint32_t start;
    uint32_t size;
    uint32_t sector_number;
} hf_flash_sector_t;

/* F407/F429-class 1 MB single-bank map (RM0090) — EDIT for your part. */
static const hf_flash_sector_t s_sector_table[] = {
    { 0x08000000U,  16U * 1024U, FLASH_SECTOR_0 },
    { 0x08004000U,  16U * 1024U, FLASH_SECTOR_1 },
    { 0x08008000U,  16U * 1024U, FLASH_SECTOR_2 },
    { 0x0800C000U,  16U * 1024U, FLASH_SECTOR_3 },
    { 0x08010000U,  64U * 1024U, FLASH_SECTOR_4 },
    { 0x08020000U, 128U * 1024U, FLASH_SECTOR_5 },
    { 0x08040000U, 128U * 1024U, FLASH_SECTOR_6 },
    { 0x08060000U, 128U * 1024U, FLASH_SECTOR_7 },
    { 0x08080000U, 128U * 1024U, FLASH_SECTOR_8 },
    { 0x080A0000U, 128U * 1024U, FLASH_SECTOR_9 },
    { 0x080C0000U, 128U * 1024U, FLASH_SECTOR_10 },
    { 0x080E0000U, 128U * 1024U, FLASH_SECTOR_11 },
};
#define HF_STM32_SECTOR_COUNT (sizeof(s_sector_table) / sizeof(s_sector_table[0]))

static const hf_flash_sector_t *hf_flash_find_sector(uint32_t address)
{
    size_t i;
    for (i = 0; i < HF_STM32_SECTOR_COUNT; i++) {
        uint32_t end = s_sector_table[i].start + s_sector_table[i].size;
        if (address >= s_sector_table[i].start && address < end) {
            return &s_sector_table[i];
        }
    }
    return NULL;
}

bool hf_flash_init(void)
{
    return true;
}

uint32_t hf_flash_erase_size(uint32_t address)
{
    const hf_flash_sector_t *sector = hf_flash_find_sector(address);
    return (sector != NULL) ? sector->size : 0U;
}

uint32_t hf_flash_write_align(void)
{
    return 4U; /* FLASH_TYPEPROGRAM_WORD — F4 reference, see file header */
}

bool hf_flash_erase(uint32_t address, uint32_t length)
{
    if (length == 0U) {
        return false;
    }

    uint32_t covered = 0U;
    bool ok = true;

    HAL_FLASH_Unlock();

    while (covered < length) {
        const hf_flash_sector_t *sector = hf_flash_find_sector(address + covered);
        if (sector == NULL || sector->start != (address + covered)) {
            /* Not aligned to the start of the erase unit at this
             * address — reject rather than erase a partial/wrong
             * sector. */
            ok = false;
            break;
        }

        FLASH_EraseInitTypeDef erase_init = {0};
        uint32_t sector_error = 0U;
        erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase_init.Sector = sector->sector_number;
        erase_init.NbSectors = 1U;
        erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
            ok = false;
            break;
        }

        covered += sector->size;
    }

    if (covered != length) {
        ok = false;
    }

    HAL_FLASH_Lock();
    return ok;
}

bool hf_flash_write(uint32_t address, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U ||
        (address % 4U) != 0U || (length % 4U) != 0U) {
        return false;
    }

    bool ok = true;
    uint32_t offset;

    HAL_FLASH_Unlock();

    for (offset = 0U; offset < length; offset += 4U) {
        uint32_t word;
        memcpy(&word, data + offset, 4U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + offset, word) != HAL_OK) {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    return ok;
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
#endif /* TARGET_STM32 */
