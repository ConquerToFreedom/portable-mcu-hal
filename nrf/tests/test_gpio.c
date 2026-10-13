/* Unit tests for nrf/gpio/hal_port_nrf52.c against tests/fakes/nrf52. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

void test_init_output_returns_true(void)
{
    TEST_ASSERT_TRUE(hf_gpio_init(5, HF_GPIO_OUTPUT_PUSHPULL));
}

void test_init_open_drain_returns_true(void)
{
    TEST_ASSERT_TRUE(hf_gpio_init(5, HF_GPIO_OUTPUT_OPENDRAIN));
}

void test_write_then_read_roundtrips(void)
{
    hf_gpio_init(12, HF_GPIO_OUTPUT_PUSHPULL);
    hf_gpio_write(12, true);
    TEST_ASSERT_TRUE(hf_gpio_read(12));
    hf_gpio_write(12, false);
    TEST_ASSERT_FALSE(hf_gpio_read(12));
}

void test_toggle_flips_state(void)
{
    hf_gpio_write(3, false);
    hf_gpio_toggle(3);
    TEST_ASSERT_TRUE(hf_gpio_read(3));
    hf_gpio_toggle(3);
    TEST_ASSERT_FALSE(hf_gpio_read(3));
}

void test_read_reflects_externally_driven_input(void)
{
    hf_gpio_init(20, HF_GPIO_INPUT_PULLUP);
    fake_nrf52_gpio_set_input(20, true);
    TEST_ASSERT_TRUE(hf_gpio_read(20));
    fake_nrf52_gpio_set_input(20, false);
    TEST_ASSERT_FALSE(hf_gpio_read(20));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_output_returns_true);
    RUN_TEST(test_init_open_drain_returns_true);
    RUN_TEST(test_write_then_read_roundtrips);
    RUN_TEST(test_toggle_flips_state);
    RUN_TEST(test_read_reflects_externally_driven_input);
    return UNITY_END();
}
