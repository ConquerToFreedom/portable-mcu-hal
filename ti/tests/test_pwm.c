/* Unit tests for ti/pwm/hal_port_ti_mspm0_pwm.c against tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(1, 3), 1000));
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(2, 0), 1000)); /* only TIMA0/TIMA1 => instance 0-1 */
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

void test_set_duty_computes_compare_value_from_period(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* 1 MHz assumed tick -> period 1000 */
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 50000)); /* 50% */
    TEST_ASSERT_EQUAL_UINT32(500, fake_ti_get_cc_value(TIMA0, DL_TIMER_CC_0_INDEX));
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
    TEST_ASSERT_EQUAL_UINT32(250, fake_ti_get_cc_value(TIMA0, DL_TIMER_CC_0_INDEX));
    TEST_ASSERT_EQUAL_UINT32(750, fake_ti_get_cc_value(TIMA0, DL_TIMER_CC_1_INDEX));
}

void test_second_instance_uses_tima1_independently(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(1, 2), 1000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(1, 2), 10000));
    TEST_ASSERT_EQUAL_UINT32(100, fake_ti_get_cc_value(TIMA1, DL_TIMER_CC_2_INDEX));
    TEST_ASSERT_EQUAL_UINT32(0, fake_ti_get_cc_value(TIMA0, DL_TIMER_CC_2_INDEX));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_rejects_zero_frequency);
    RUN_TEST(test_start_and_stop_return_true_for_valid_channel);
    RUN_TEST(test_set_duty_computes_compare_value_from_period);
    RUN_TEST(test_set_duty_rejects_out_of_range_value);
    RUN_TEST(test_independent_channels_on_same_instance_get_independent_duty);
    RUN_TEST(test_second_instance_uses_tima1_independently);
    return UNITY_END();
}
