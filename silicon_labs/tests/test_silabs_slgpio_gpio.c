/* Unit tests for silicon_labs/gpio/hal_port_silabs_slgpio.c
 * (TARGET_SILABS_SLGPIO, current sl_gpio) against
 * tests/fakes/silabs_slgpio. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs_slgpio.h"

void setUp(void) { fake_silabs_slgpio_reset(); }
void tearDown(void) {}

void test_init_returns_true_for_every_valid_mode(void)
{
    hf_gpio_t pin = HF_PIN(SL_GPIO_PORT_A, 3);
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_INPUT));
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_INPUT_PULLUP));
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_INPUT_PULLDOWN));
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_OUTPUT_PUSHPULL));
    TEST_ASSERT_TRUE(hf_gpio_init(pin, HF_GPIO_OUTPUT_OPENDRAIN));
}

void test_write_then_read_roundtrips(void)
{
    hf_gpio_t pin = HF_PIN(SL_GPIO_PORT_B, 5);
    hf_gpio_write(pin, true);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    hf_gpio_write(pin, false);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

void test_toggle_flips_state(void)
{
    hf_gpio_t pin = HF_PIN(SL_GPIO_PORT_C, 7);
    hf_gpio_write(pin, false);
    hf_gpio_toggle(pin);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    hf_gpio_toggle(pin);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

void test_read_reflects_externally_driven_input(void)
{
    hf_gpio_t pin = HF_PIN(SL_GPIO_PORT_D, 2);
    fake_silabs_slgpio_gpio_set_input(SL_GPIO_PORT_D, 2, true);
    TEST_ASSERT_TRUE(hf_gpio_read(pin));
    fake_silabs_slgpio_gpio_set_input(SL_GPIO_PORT_D, 2, false);
    TEST_ASSERT_FALSE(hf_gpio_read(pin));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_true_for_every_valid_mode);
    RUN_TEST(test_write_then_read_roundtrips);
    RUN_TEST(test_toggle_flips_state);
    RUN_TEST(test_read_reflects_externally_driven_input);
    return UNITY_END();
}
