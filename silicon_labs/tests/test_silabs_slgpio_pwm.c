/* Unit tests for silicon_labs/pwm/hal_port_silabs_slgpio_pwm.c
 * (TARGET_SILABS_SLGPIO, sl_pwm) against tests/fakes/silabs_slgpio.
 *
 * sl_pwm_instance_t is adapter-private; tests identify an instance by
 * slot number (the Nth distinct sl_pwm_init() call, in hf_pwm_init()
 * call order) - see fake_silabs_slgpio.h's own comment on this.
 */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs_slgpio.h"

void setUp(void) { fake_silabs_slgpio_reset(); }
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

void test_operations_fail_before_init(void)
{
    /* Uses a channel no earlier test in this file touches - the
     * adapter's own s_valid[][] state is static and NOT reset by
     * fake_silabs_slgpio_reset(), so reusing an already-init'd channel
     * here would find it valid from a previous test's leftover state. */
    TEST_ASSERT_FALSE(hf_pwm_start(HF_PWM(1, 1)));
    TEST_ASSERT_FALSE(hf_pwm_set_duty(HF_PWM(1, 1), 50000));
}

void test_start_and_stop_track_running_state(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* slot 0 */
    TEST_ASSERT_FALSE(fake_sl_pwm_is_running(0));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    TEST_ASSERT_TRUE(fake_sl_pwm_is_running(0));
    hf_pwm_stop(HF_PWM(0, 0));
    TEST_ASSERT_FALSE(fake_sl_pwm_is_running(0));
}

void test_set_duty_rounds_to_nearest_whole_percent(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* slot 0 */
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 12345)); /* 12.345% -> rounds to 12% */
    TEST_ASSERT_EQUAL_UINT8(12, fake_sl_pwm_get_duty_cycle(0));
}

void test_set_duty_rejects_out_of_range_value(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_FALSE(hf_pwm_set_duty(HF_PWM(0, 0), 100001));
}

void test_independent_channels_get_independent_slots_and_duty(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* slot 0 */
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 1), 1000)); /* slot 1 */
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 25000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 1), 75000));
    TEST_ASSERT_EQUAL_UINT8(25, fake_sl_pwm_get_duty_cycle(0));
    TEST_ASSERT_EQUAL_UINT8(75, fake_sl_pwm_get_duty_cycle(1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_rejects_zero_frequency);
    RUN_TEST(test_operations_fail_before_init);
    RUN_TEST(test_start_and_stop_track_running_state);
    RUN_TEST(test_set_duty_rounds_to_nearest_whole_percent);
    RUN_TEST(test_set_duty_rejects_out_of_range_value);
    RUN_TEST(test_independent_channels_get_independent_slots_and_duty);
    return UNITY_END();
}
