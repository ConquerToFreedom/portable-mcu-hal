/* Unit tests for ti/flash/hal_port_ti_mspm0_flash.c against
 * tests/fakes/ti_mspm0. DL_FlashCTL_eraseMemory/programMemoryBlocking
 * never touch real memory in the fake, so arbitrary sector-aligned
 * addresses are safe — only hf_flash_read()'s raw memcpy is a real
 * memory access, and that one test uses a MAP_32BIT mapping since the
 * real API takes a 32-bit address but this suite runs on a 64-bit
 * host (see the comment on that test).
 */
#define _GNU_SOURCE
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"
#include <sys/mman.h>

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

void test_init_returns_true(void)
{
    TEST_ASSERT_TRUE(hf_flash_init());
}

void test_erase_size_is_1024_regardless_of_address(void)
{
    TEST_ASSERT_EQUAL_UINT32(1024U, hf_flash_erase_size(0U));
    TEST_ASSERT_EQUAL_UINT32(1024U, hf_flash_erase_size(0x7C00U));
}

void test_write_align_is_8_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT32(8U, hf_flash_write_align());
}

void test_erase_one_sector_sequence(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x400U, 1024U));
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_clear_status_count);
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_unprotect_count);
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x400U, fake_ti_flash_last_erase_address);
}

void test_erase_spanning_two_sectors_calls_erase_twice(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x400U, 2048U));
    TEST_ASSERT_EQUAL_INT(2, fake_ti_flash_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x800U, fake_ti_flash_last_erase_address);
}

void test_erase_rejects_misaligned_address_or_length(void)
{
    TEST_ASSERT_FALSE(hf_flash_erase(0x401U, 1024U));
    TEST_ASSERT_FALSE(hf_flash_erase(0x400U, 100U));
    TEST_ASSERT_EQUAL_INT(0, fake_ti_flash_erase_call_count);
}

void test_erase_fails_when_wait_for_cmd_done_fails(void)
{
    fake_ti_flash_wait_done_result = false;
    TEST_ASSERT_FALSE(hf_flash_erase(0x400U, 1024U));
}

void test_write_calls_program_memory_blocking(void)
{
    uint8_t data[8] = {0};
    TEST_ASSERT_TRUE(hf_flash_write(0x400U, data, 8U));
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_clear_status_count);
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_unprotect_count);
    TEST_ASSERT_EQUAL_INT(1, fake_ti_flash_program_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x400U, fake_ti_flash_last_program_address);
    TEST_ASSERT_EQUAL_UINT32(2U, fake_ti_flash_last_program_data_size);
}

void test_write_rejects_unaligned_address_or_length(void)
{
    uint8_t data[8] = {0};
    TEST_ASSERT_FALSE(hf_flash_write(0x401U, data, 8U));
    TEST_ASSERT_FALSE(hf_flash_write(0x400U, data, 4U));
    TEST_ASSERT_FALSE(hf_flash_write(0x400U, NULL, 8U));
}

void test_write_fails_on_program_error(void)
{
    uint8_t data[8] = {0};
    fake_ti_flash_program_result = false;
    TEST_ASSERT_FALSE(hf_flash_write(0x400U, data, 8U));
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
    TEST_ASSERT_FALSE(hf_flash_read(0x400U, NULL, 4U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_true);
    RUN_TEST(test_erase_size_is_1024_regardless_of_address);
    RUN_TEST(test_write_align_is_8_bytes);
    RUN_TEST(test_erase_one_sector_sequence);
    RUN_TEST(test_erase_spanning_two_sectors_calls_erase_twice);
    RUN_TEST(test_erase_rejects_misaligned_address_or_length);
    RUN_TEST(test_erase_fails_when_wait_for_cmd_done_fails);
    RUN_TEST(test_write_calls_program_memory_blocking);
    RUN_TEST(test_write_rejects_unaligned_address_or_length);
    RUN_TEST(test_write_fails_on_program_error);
    RUN_TEST(test_read_copies_from_given_address);
    RUN_TEST(test_read_rejects_null_buffer);
    return UNITY_END();
}
