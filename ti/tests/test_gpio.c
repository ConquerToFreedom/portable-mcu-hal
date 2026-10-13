/* Unit tests for ti/gpio/hal_port_ti_mspm0.c against
 * tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

void test_init_direction_only(void)
{
    hf_gpio_t pin = HF_PIN(0, 3);
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_OUTPUT_PUSHPULL));
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_INPUT));
}

void test_init_pull_and_open_drain_are_unreachable(void)
{
    /* Genuine, documented MSPM0 limitation - pull/open-drain live in
     * IOMUX PINCM, not reachable from hf_gpio_t alone. */
    hf_gpio_t pin = HF_PIN(0, 3);
    TEST_ASSERT_FALSE(hf_gpio_init(pin, HF_GPIO_INPUT_PULLUP));
    TEST_ASSERT_FALSE(hf_gpio_init(pin, HF_GPIO_INPUT_PULLDOWN));
    TEST_ASSERT_FALSE(hf_gpio_init(pin, HF_GPIO_OUTPUT_OPENDRAIN));
}

void test_init_port_beyond_table_returns_false(void)
{
    hf_gpio_t pin = HF_PIN(2, 0); /* only ports 0-1 (A/B) exist on MSPM0 */
    TEST_ASSERT_FALSE(hf_gpio_init(pin, HF_GPIO_OUTPUT_PUSHPULL));
}

void test_write_then_read_roundtrips(void)
{
    hf_gpio_t pin = HF_PIN(1, 5);
    hf_gpio_write(pin, true);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    hf_gpio_write(pin, false);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

void test_toggle_flips_state(void)
{
    hf_gpio_t pin = HF_PIN(0, 7);
    hf_gpio_write(pin, false);
    hf_gpio_toggle(pin);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    hf_gpio_toggle(pin);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

void test_read_reflects_externally_driven_input(void)
{
    hf_gpio_t pin = HF_PIN(0, 9);
    fake_ti_gpio_set_input(GPIOA, (1U << 9), true);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    fake_ti_gpio_set_input(GPIOA, (1U << 9), false);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_direction_only);
    RUN_TEST(test_init_pull_and_open_drain_are_unreachable);
    RUN_TEST(test_init_port_beyond_table_returns_false);
    RUN_TEST(test_write_then_read_roundtrips);
    RUN_TEST(test_toggle_flips_state);
    RUN_TEST(test_read_reflects_externally_driven_input);
    return UNITY_END();
}
