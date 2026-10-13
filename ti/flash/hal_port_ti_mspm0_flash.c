/* TI MSPM0 (driverlib) vendor adapter for the Flash portion of
 * hal_wrapper.h, against DL_FlashCTL.
 *
 * Verified directly against TexasInstruments/mspm0-sdk's own
 * source/ti/driverlib/dl_flashctl.h on GitHub (TI's docs domain and
 * forum are both blocked in this environment, same higher-confidence-
 * source fallback this vendor's ADC/SPI adapters already used) —
 * confirmed: DL_FLASHCTL_SECTOR_SIZE is 1024 bytes; flash words are
 * 64-bit (addresses passed to DL_FlashCTL_setCommandAddress/erase/
 * program must be flash-word aligned); DL_FlashCTL_eraseMemory(flashctl,
 * address, DL_FLASHCTL_COMMAND_SIZE_SECTOR) starts a sector erase and
 * returns immediately — DL_FlashCTL_waitForCmdDone(flashctl) must be
 * polled afterward to block until it finishes; DL_FlashCTL_unprotectSector
 * (flashctl, addr, DL_FLASHCTL_REGION_SELECT_MAIN) must be called
 * before programming ("Before programming memory, the user must
 * unprotect the region of memory to program" — the header's own
 * wording) and the flash controller re-protects all memory after
 * every program completes, so this must run before every write, not
 * just once; DL_FlashCTL_programMemoryBlocking(flashctl, address,
 * uint32_t *data, dataSize, regionSelect) is the plain (non-ECC)
 * blocking program call, `dataSize` a count of 32-bit words, writing
 * 64-bit words two at a time.
 *
 * NOT independently confirmed in this fetch pass (the file is 3160
 * lines; GitHub's non-JS rendering only exposed roughly the first
 * third and a middle slice, not the whole thing) — flagged rather
 * than guessed, same treatment as this vendor's Timer adapter's own
 * flagged list:
 *   1. `FLASHCTL` — the peripheral base-pointer macro name passed as
 *      this file's `FLASHCTL_Regs *flashctl` argument. Assumed by the
 *      same single-instance-peripheral naming convention already
 *      confirmed elsewhere on this vendor (GPIOA, TIMG0), not directly
 *      seen in the fetched excerpts.
 *   2. Whether DL_FlashCTL_unprotectSector must also precede
 *      DL_FlashCTL_eraseMemory, not just the program calls — the
 *      fetched excerpt's explicit "@pre" note was on the program
 *      functions only. This file calls it before erase too, on the
 *      assumption that erase is at least as destructive as program
 *      and the controller's write/erase protection is almost
 *      certainly one combined mechanism, not two — cross-check your
 *      installed SDK's dl_flashctl.h before relying on this.
 *   3. DL_FlashCTL_executeClearStatus(flashctl) — this file calls it
 *      once per operation before unprotectSector, matching its own
 *      doc comment ("should be called before unprotecting so memory
 *      isn't re-protected afterward"); not independently re-verified
 *      beyond that one comment.
 *
 * Per hal_wrapper.h's hf_flash_t header comment: this file uses the
 * normal flash-resident DL_FlashCTL_eraseMemory/programMemoryBlocking
 * calls, deliberately NOT the "FromRAM" variants driverlib also ships
 * — never point hf_flash_erase()/hf_flash_write() at the sector(s)
 * containing your own running code or vector table; that is exactly
 * the case the FromRAM variants exist for and this adapter does not
 * implement (backlog, see hal_wrapper.h).
 */
#include "hal_wrapper.h"

#if defined(TARGET_TI_MSPM0)
#if HF_ENABLE_FLASH

#include <string.h>
#include "ti_msp_dl_config.h"

bool hf_flash_init(void)
{
    return true;
}

uint32_t hf_flash_erase_size(uint32_t address)
{
    (void)address;
    return DL_FLASHCTL_SECTOR_SIZE;
}

uint32_t hf_flash_write_align(void)
{
    return 8U; /* 64-bit flash word — see file header */
}

bool hf_flash_erase(uint32_t address, uint32_t length)
{
    if (length == 0U ||
        (address % DL_FLASHCTL_SECTOR_SIZE) != 0U ||
        (length % DL_FLASHCTL_SECTOR_SIZE) != 0U) {
        return false;
    }

    uint32_t offset;
    for (offset = 0U; offset < length; offset += DL_FLASHCTL_SECTOR_SIZE) {
        DL_FlashCTL_executeClearStatus(FLASHCTL);
        DL_FlashCTL_unprotectSector(FLASHCTL, address + offset, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FlashCTL_eraseMemory(FLASHCTL, address + offset, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
        if (!DL_FlashCTL_waitForCmdDone(FLASHCTL)) {
            return false;
        }
    }
    return true;
}

bool hf_flash_write(uint32_t address, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U ||
        (address % 8U) != 0U || (length % 8U) != 0U) {
        return false;
    }

    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, address, DL_FLASHCTL_REGION_SELECT_MAIN);
    return DL_FlashCTL_programMemoryBlocking(FLASHCTL, address, (uint32_t *)data,
                                              length / 4U, DL_FLASHCTL_REGION_SELECT_MAIN);
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
#endif /* TARGET_TI_MSPM0 */
