/* Unit tests for silicon_labs/pwm/hal_port_silabs_pwm.c
 * (TARGET_SILABS, classic emlib) against tests/fakes/silabs. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"

void setUp(void) { fake_silabs_reset(); }
void tearDown(void) {}

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(1, 3), 1000));
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(2, 0), 1000)); /* only TIMER2/TIMER3 => instance 0-1 */
}

void test_init_rejects_zero_frequency(void)
{
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(0, 0), 0));
}

void test_start_and_stop_return_true_for_valid_channel(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    hf_pwm_stop(HF_PWM(0, 0)); /* void - just confirm it doesn't crash */
}

void test_init_writes_the_gpio_timerroute_for_its_channel(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 1), 1000)); /* TIMER2, CC1 -> port A (0), pin 1 - see EDIT ME table */
    uint32_t route = fake_silabs_pwm_get_route(TIMER2, 1);
    TEST_ASSERT_EQUAL_UINT32(1U << 8, route); /* pin 1 in the PIN field, port A == 0 in the PORT field */
}

void test_start_enables_the_route_for_its_channel(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 2), 1000));
    TEST_ASSERT_FALSE(fake_silabs_pwm_route_enabled(TIMER2, 2));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 2)));
    TEST_ASSERT_TRUE(fake_silabs_pwm_route_enabled(TIMER2, 2));
}

void test_set_duty_computes_compare_value_from_top(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* 1 MHz assumed HFPERCLK -> top 1000 */
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 50000)); /* 50% */
    TEST_ASSERT_EQUAL_UINT32(500, fake_silabs_pwm_get_compare(TIMER2, 0));
}

void test_set_duty_rejects_out_of_range_value(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_FALSE(hf_pwm_set_duty(HF_PWM(0, 0), 100001));
}

void test_independent_channels_on_same_instance_get_independent_duty(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 1), 1000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 25000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 1), 75000));
    TEST_ASSERT_EQUAL_UINT32(250, fake_silabs_pwm_get_compare(TIMER2, 0));
    TEST_ASSERT_EQUAL_UINT32(750, fake_silabs_pwm_get_compare(TIMER2, 1));
}

void test_second_instance_uses_timer3_independently(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(1, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(1, 0), 10000));
    TEST_ASSERT_EQUAL_UINT32(100, fake_silabs_pwm_get_compare(TIMER3, 0));
    TEST_ASSERT_EQUAL_UINT32(0, fake_silabs_pwm_get_compare(TIMER2, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_rejects_zero_frequency);
    RUN_TEST(test_start_and_stop_return_true_for_valid_channel);
    RUN_TEST(test_init_writes_the_gpio_timerroute_for_its_channel);
    RUN_TEST(test_start_enables_the_route_for_its_channel);
    RUN_TEST(test_set_duty_computes_compare_value_from_top);
    RUN_TEST(test_set_duty_rejects_out_of_range_value);
    RUN_TEST(test_independent_channels_on_same_instance_get_independent_duty);
    RUN_TEST(test_second_instance_uses_timer3_independently);
    return UNITY_END();
}
