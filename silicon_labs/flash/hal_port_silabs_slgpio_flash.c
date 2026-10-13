/* Silicon Labs current-SDK (sl_gpio/sl_i2c/IADC-generation) vendor
 * adapter for the Flash portion of hal_wrapper.h.
 *
 * This file is, deliberately, near-identical to this vendor's classic
 * (TARGET_SILABS) flash adapter — a genuine finding, not an oversight:
 * unlike GPIO/I2C/ADC, where the current SDK ships a real, distinct
 * sl_gpio/sl_i2c/IADC driver layer over different peripheral hardware,
 * and unlike PWM, where sl_pwm is at least a thin current-generation
 * convenience driver over classic TIMER (see hal_wrapper.h's hf_pwm_t
 * comment), there is no current-SDK driver at this level for raw
 * flash at all. Silicon Labs' current-generation flash story is NVM3
 * (a wear-leveled key-value store) sitting ON TOP of the same classic
 * MSC (Memory System Controller) peripheral this vendor's classic
 * adapter uses directly — and hf_flash_t is explicitly scoped to raw
 * flash access, not a filesystem/KV-store (see hal_wrapper.h's own
 * "deliberately NOT in v1" backlog note), so there is nothing for this
 * target to reach for underneath NVM3 except the same MSC calls the
 * classic target uses. Both targets therefore use em_msc.h directly —
 * documented here rather than silently duplicating the classic file
 * with no explanation of why they're the same.
 *
 * See hal_port_silabs_flash.c's header comment for the MSC API
 * details, the flagged msc_Return_TypeDef uncertainty, the
 * HF_SILABS_FLASH_PAGE_SIZE EDIT-ME note, and the RAM-executed-write
 * safety rule — all identical on this target.
 */
#include "hal_wrapper.h"

#if defined(TARGET_SILABS_SLGPIO)
#if HF_ENABLE_FLASH

#include <string.h>
#include "em_msc.h"

#define HF_SILABS_FLASH_PAGE_SIZE 2048U /* EDIT for your part — see hal_port_silabs_flash.c */

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
#endif /* TARGET_SILABS_SLGPIO */
