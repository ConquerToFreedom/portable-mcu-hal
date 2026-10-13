/* Unit tests for nrf/flash/hal_port_nrf52_flash.c against
 * tests/fakes/nrf52. nrfx_nvmc_page_erase/words_write never touch real
 * memory in the fake, so arbitrary page-aligned addresses are safe —
 * only hf_flash_read()'s raw memcpy is a real memory access, and that
 * one test uses a MAP_32BIT mapping since the real API takes a 32-bit
 * address but this suite runs on a 64-bit host (see the comment on
 * that test).
 */
#define _GNU_SOURCE
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"
#include <sys/mman.h>

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

void test_init_returns_true(void)
{
    TEST_ASSERT_TRUE(hf_flash_init());
}

void test_erase_size_is_4096_regardless_of_address(void)
{
    TEST_ASSERT_EQUAL_UINT32(4096U, hf_flash_erase_size(0U));
    TEST_ASSERT_EQUAL_UINT32(4096U, hf_flash_erase_size(0x7F000U));
}

void test_write_align_is_4_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT32(4U, hf_flash_write_align());
}

void test_erase_one_page_calls_nvmc_erase_once(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x1000U, 4096U));
    TEST_ASSERT_EQUAL_INT(1, fake_nrf52_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x1000U, fake_nrf52_last_erase_address);
}

void test_erase_spanning_two_pages_calls_nvmc_erase_twice(void)
{
    TEST_ASSERT_TRUE(hf_flash_erase(0x1000U, 8192U));
    TEST_ASSERT_EQUAL_INT(2, fake_nrf52_erase_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x2000U, fake_nrf52_last_erase_address);
}

void test_erase_rejects_misaligned_address_or_length(void)
{
    TEST_ASSERT_FALSE(hf_flash_erase(0x1001U, 4096U));
    TEST_ASSERT_FALSE(hf_flash_erase(0x1000U, 100U));
    TEST_ASSERT_EQUAL_INT(0, fake_nrf52_erase_call_count);
}

void test_erase_fails_on_nvmc_error(void)
{
    fake_nrf52_next_status = NRFX_ERROR_INTERNAL;
    TEST_ASSERT_FALSE(hf_flash_erase(0x1000U, 4096U));
}

void test_write_calls_nvmc_words_write(void)
{
    uint8_t data[8] = {0};
    TEST_ASSERT_TRUE(hf_flash_write(0x1000U, data, 8U));
    TEST_ASSERT_EQUAL_INT(1, fake_nrf52_write_call_count);
    TEST_ASSERT_EQUAL_UINT32(0x1000U, fake_nrf52_last_write_address);
    TEST_ASSERT_EQUAL_UINT32(2U, fake_nrf52_last_write_num_words);
}

void test_write_rejects_unaligned_address_or_length(void)
{
    uint8_t data[4] = {0};
    TEST_ASSERT_FALSE(hf_flash_write(0x1001U, data, 4U));
    TEST_ASSERT_FALSE(hf_flash_write(0x1000U, data, 3U));
    TEST_ASSERT_FALSE(hf_flash_write(0x1000U, NULL, 4U));
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
    TEST_ASSERT_FALSE(hf_flash_read(0x1000U, NULL, 4U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_true);
    RUN_TEST(test_erase_size_is_4096_regardless_of_address);
    RUN_TEST(test_write_align_is_4_bytes);
    RUN_TEST(test_erase_one_page_calls_nvmc_erase_once);
    RUN_TEST(test_erase_spanning_two_pages_calls_nvmc_erase_twice);
    RUN_TEST(test_erase_rejects_misaligned_address_or_length);
    RUN_TEST(test_erase_fails_on_nvmc_error);
    RUN_TEST(test_write_calls_nvmc_words_write);
    RUN_TEST(test_write_rejects_unaligned_address_or_length);
    RUN_TEST(test_read_copies_from_given_address);
    RUN_TEST(test_read_rejects_null_buffer);
    return UNITY_END();
}
