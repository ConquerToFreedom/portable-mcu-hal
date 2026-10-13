/* Unit tests for silicon_labs/flash/hal_port_silabs_flash.c
 * (TARGET_SILABS, classic emlib) against tests/fakes/silabs.
 * MSC_ErasePage/WriteWord never touch real memory in the fake, so
 * arbitrary page-aligned addresses are safe — only hf_flash_read()'s
 * raw memcpy is a real memory access, and that one test uses a
 * MAP_32BIT mapping since the real API takes a 32-bit address but
 * this suite runs on a 64-bit host (see the comment on that test).
 */
#define _GNU_SOURCE
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"
#include <sys/mman.h>

void setUp(void) { fake_silabs_reset(); }
void tearDown(void) {}

void test_init_calls_msc_init(void)
{
    TEST_ASSERT_TRUE(hf_flash_init());
    TEST_ASSERT_EQUAL_INT(1, fake_silabs_msc_init_count);
}

void test_erase_size_is_2048_regardless_of_address(void)
{
    TEST_ASSERT_EQUAL_UINT32(2048U, hf_flash_erase_size(0U));
    TEST_ASSERT_EQUAL_UINT32(2048U, hf_flash_erase_size(0x7F000U));
}

void test_write_align_is_4_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT32(4U, hf_flash_write_align());
}

void test_erase_one_page_calls_msc_erase_once(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x800U, 2048U));
    TEST_ASSERT_EQUAL_INT(1, fake_silabs_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x800U, fake_silabs_last_erase_address);
}

void test_erase_spanning_two_pages_calls_msc_erase_twice(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x800U, 4096U));
    TEST_ASSERT_EQUAL_INT(2, fake_silabs_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x1000U, fake_silabs_last_erase_address);
}

void test_erase_rejects_misaligned_address_or_length(void)
{
    TEST_ASSERT_FALSE(hf_flash_erase(0x801U, 2048U));
    TEST_ASSERT_FALSE(hf_flash_erase(0x800U, 100U));
    TEST_ASSERT_EQUAL_INT(0, fake_silabs_erase_call_count);
}

void test_erase_fails_on_msc_error(void)
{
    fake_silabs_msc_next_status = mscReturnTimeOut;
    TEST_ASSERT_FALSE(hf_flash_erase(0x800U, 2048U));
}

void test_write_calls_msc_write_word(void)
{
    uint8_t data[8] = {0};
    TEST_ASSERT_TRUE(hf_flash_write(0x800U, data, 8U));
    TEST_ASSERT_EQUAL_INT(1, fake_silabs_write_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x800U, fake_silabs_last_write_address);
    TEST_ASSERT_EQUAL_UINT32(8U, fake_silabs_last_write_num_bytes);
}

void test_write_rejects_unaligned_address_or_length(void)
{
    uint8_t data[4] = {0};
    TEST_ASSERT_FALSE(hf_flash_write(0x801U, data, 4U));
    TEST_ASSERT_FALSE(hf_flash_write(0x800U, data, 3U));
    TEST_ASSERT_FALSE(hf_flash_write(0x800U, NULL, 4U));
}

void test_write_fails_on_msc_error(void)
{
    uint8_t data[4] = {0};
    fake_silabs_msc_next_status = mscReturnLocked;
    TEST_ASSERT_FALSE(hf_flash_write(0x800U, data, 4U));
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
    TEST_ASSERT_FALSE(hf_flash_read(0x800U, NULL, 4U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_calls_msc_init);
    RUN_TEST(test_erase_size_is_2048_regardless_of_address);
    RUN_TEST(test_write_align_is_4_bytes);
    RUN_TEST(test_erase_one_page_calls_msc_erase_once);
    RUN_TEST(test_erase_spanning_two_pages_calls_msc_erase_twice);
    RUN_TEST(test_erase_rejects_misaligned_address_or_length);
    RUN_TEST(test_erase_fails_on_msc_error);
    RUN_TEST(test_write_calls_msc_write_word);
    RUN_TEST(test_write_rejects_unaligned_address_or_length);
    RUN_TEST(test_write_fails_on_msc_error);
    RUN_TEST(test_read_copies_from_given_address);
    RUN_TEST(test_read_rejects_null_buffer);
    return UNITY_END();
}
