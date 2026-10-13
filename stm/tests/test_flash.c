/* Unit tests for stm/flash/hal_port_stm32_flash.c against
 * tests/fakes/stm32. HAL_FLASHEx_Erase/HAL_FLASH_Program never touch
 * real memory in the fake (see fake_stm32_hal.c), so these addresses
 * are safe on a host even though they match real STM32 F4 flash
 * addresses — only hf_flash_read()'s raw memcpy is a real memory
 * access, so that one test uses a local buffer's own address instead.
 */
#define _GNU_SOURCE
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

/* hf_flash_read() takes a 32-bit address by design (every target this
 * project supports is a 32-bit MCU), but this test suite runs on a
 * 64-bit host — truncating an ordinary 64-bit stack/static pointer
 * down to uint32_t for the call would silently point at garbage and
 * segfault (found the hard way while writing this test). MAP_32BIT
 * asks Linux to place this one mapping below the 4 GB line so the
 * truncation is lossless — a host-test-only trick, not something any
 * adapter or the API itself does. */
#include <sys/mman.h>

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

void test_init_returns_true(void)
{
    TEST_ASSERT_TRUE(hf_flash_init());
}

void test_erase_size_matches_f4_sector_table(void)
{
    TEST_ASSERT_EQUAL_UINT32(16U * 1024U, hf_flash_erase_size(0x08000000U)); /* sector 0 */
    TEST_ASSERT_EQUAL_UINT32(64U * 1024U, hf_flash_erase_size(0x08010000U)); /* sector 4 */
    TEST_ASSERT_EQUAL_UINT32(128U * 1024U, hf_flash_erase_size(0x08020000U)); /* sector 5 */
    TEST_ASSERT_EQUAL_UINT32(0U, hf_flash_erase_size(0xFFFFFFFFU)); /* out of range */
}

void test_write_align_is_4_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT32(4U, hf_flash_write_align());
}

void test_erase_one_sector_calls_hal_erase_once(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x08000000U, 16U * 1024U));
    TEST_ASSERT_EQUAL_INT(1, fake_stm32_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(FLASH_SECTOR_0, fake_stm32_last_erase_sector);
    TEST_ASSERT_EQUAL_INT(1, fake_stm32_flash_unlock_count);
    TEST_ASSERT_EQUAL_INT(1, fake_stm32_flash_lock_count);
}

void test_erase_spanning_two_sectors_calls_hal_erase_twice(void)
{
    /* Sectors 0 and 1 are each 16 KB, contiguous. */
    TEST_ASSERT_TRUE(hf_flash_erase(0x08000000U, 32U * 1024U));
    TEST_ASSERT_EQUAL_INT(2, fake_stm32_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(FLASH_SECTOR_1, fake_stm32_last_erase_sector);
}

void test_erase_rejects_misaligned_address(void)
{
    TEST_ASSERT_FALSE(hf_flash_erase(0x08000001U, 16U * 1024U));
    TEST_ASSERT_EQUAL_INT(0, fake_stm32_erase_call_count);
}

void test_erase_rejects_length_not_matching_sector_size(void)
{
    TEST_ASSERT_FALSE(hf_flash_erase(0x08000000U, 1024U));
}

void test_erase_fails_on_hal_error(void)
{
    fake_stm32_next_status = HAL_ERROR;
    TEST_ASSERT_FALSE(hf_flash_erase(0x08000000U, 16U * 1024U));
}

void test_write_programs_each_word(void)
{
    uint8_t data[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    TEST_ASSERT_TRUE(hf_flash_write(0x08000000U, data, 8U));
    TEST_ASSERT_EQUAL_INT(2, fake_stm32_program_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x08000004U, fake_stm32_last_program_address);
}

void test_write_rejects_unaligned_address_or_length(void)
{
    uint8_t data[4] = {0};
    TEST_ASSERT_FALSE(hf_flash_write(0x08000001U, data, 4U));
    TEST_ASSERT_FALSE(hf_flash_write(0x08000000U, data, 3U));
    TEST_ASSERT_FALSE(hf_flash_write(0x08000000U, NULL, 4U));
}

void test_write_fails_on_hal_error(void)
{
    uint8_t data[4] = {0};
    fake_stm32_next_status = HAL_ERROR;
    TEST_ASSERT_FALSE(hf_flash_write(0x08000000U, data, 4U));
}

void test_read_copies_from_given_address(void)
{
    uint8_t *source = mmap(NULL, 4096U, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    TEST_ASSERT_NOT_EQUAL(MAP_FAILED, source);
    source[0] = 0xAA; source[1] = 0xBB; source[2] = 0xCC; source[3] = 0xDD;

    uint8_t out[4] = {0};
    TEST_ASSERT_TRUE(hf_flash_read((uint32_t)(uintptr_t)source, out, 4U));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source, out, 4);

    munmap(source, 4096U);
}

void test_read_rejects_null_buffer(void)
{
    TEST_ASSERT_FALSE(hf_flash_read(0x08000000U, NULL, 4U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_true);
    RUN_TEST(test_erase_size_matches_f4_sector_table);
    RUN_TEST(test_write_align_is_4_bytes);
    RUN_TEST(test_erase_one_sector_calls_hal_erase_once);
    RUN_TEST(test_erase_spanning_two_sectors_calls_hal_erase_twice);
    RUN_TEST(test_erase_rejects_misaligned_address);
    RUN_TEST(test_erase_rejects_length_not_matching_sector_size);
    RUN_TEST(test_erase_fails_on_hal_error);
    RUN_TEST(test_write_programs_each_word);
    RUN_TEST(test_write_rejects_unaligned_address_or_length);
    RUN_TEST(test_write_fails_on_hal_error);
    RUN_TEST(test_read_copies_from_given_address);
    RUN_TEST(test_read_rejects_null_buffer);
    return UNITY_END();
}
