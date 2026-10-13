/* Unit tests for nrf/pwm/hal_port_nrf52_pwm.c against tests/fakes/nrf52. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(1, 2), 1000));
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(2, 0), 1000)); /* only PWM0/PWM1 => instance 0-1 */
}

void test_init_rejects_zero_frequency(void)
{
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(0, 0), 0));
}

void test_init_fails_when_nrfx_reports_error(void)
{
    fake_nrf52_next_status = NRFX_ERROR_INTERNAL;
    TEST_ASSERT_FALSE(hf_pwm_init(HF_PWM(0, 0), 1000));
}

void test_set_duty_before_start_does_not_trigger_playback(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 50000));
    TEST_ASSERT_FALSE(fake_nrf52_pwm_is_playing(0));
    TEST_ASSERT_NULL(fake_nrf52_pwm_last_sequence(0));
}

void test_start_begins_looped_playback(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    TEST_ASSERT_TRUE(fake_nrf52_pwm_is_playing(0));
}

void test_set_duty_while_running_updates_sequence_with_exact_ticks(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000)); /* 1 MHz base -> top_value 1000 */
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 50000)); /* 50% */

    nrf_pwm_sequence_t const *seq = fake_nrf52_pwm_last_sequence(0);
    TEST_ASSERT_NOT_NULL(seq);
    TEST_ASSERT_EQUAL_UINT16(500, seq->values.p_individual->channel_0);
}

void test_independent_channels_on_same_instance_get_independent_duty(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 1), 1000));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 0), 25000));
    TEST_ASSERT_TRUE(hf_pwm_set_duty(HF_PWM(0, 1), 75000));

    nrf_pwm_sequence_t const *seq = fake_nrf52_pwm_last_sequence(0);
    TEST_ASSERT_EQUAL_UINT16(250, seq->values.p_individual->channel_0);
    TEST_ASSERT_EQUAL_UINT16(750, seq->values.p_individual->channel_1);
}

void test_set_duty_rejects_out_of_range_value(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_FALSE(hf_pwm_set_duty(HF_PWM(0, 0), 100001));
}

void test_stop_clears_playing_state(void)
{
    TEST_ASSERT_TRUE(hf_pwm_init(HF_PWM(0, 0), 1000));
    TEST_ASSERT_TRUE(hf_pwm_start(HF_PWM(0, 0)));
    hf_pwm_stop(HF_PWM(0, 0));
    TEST_ASSERT_FALSE(fake_nrf52_pwm_is_playing(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_rejects_zero_frequency);
    RUN_TEST(test_init_fails_when_nrfx_reports_error);
    RUN_TEST(test_set_duty_before_start_does_not_trigger_playback);
    RUN_TEST(test_start_begins_looped_playback);
    RUN_TEST(test_set_duty_while_running_updates_sequence_with_exact_ticks);
    RUN_TEST(test_independent_channels_on_same_instance_get_independent_duty);
    RUN_TEST(test_set_duty_rejects_out_of_range_value);
    RUN_TEST(test_stop_clears_playing_state);
    return UNITY_END();
}
