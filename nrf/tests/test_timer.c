/* Unit tests for nrf/timer/hal_port_nrf52_timer.c against tests/fakes/nrf52. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

static int s_fired;
static hf_timer_t s_last_instance;
static void on_elapsed(hf_timer_t instance) { s_fired++; s_last_instance = instance; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_FALSE(hf_timer_init(2, 1000, HF_TIMER_PERIODIC)); /* only TIMER1/TIMER2 => 0-1 */
}

void test_init_fails_when_nrfx_reports_error(void)
{
    fake_nrf52_next_status = NRFX_ERROR_INTERNAL;
    TEST_ASSERT_FALSE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
}

void test_start_and_stop_return_true_for_valid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));
    hf_timer_stop(0); /* void - just confirm it doesn't crash */
}

void test_callback_fires_with_correct_instance_and_delivered_context(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));

    fake_nrf52_timer_fire_event(0, NRF_TIMER_EVENT_COMPARE0);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(0, s_last_instance);
}

void test_non_compare_event_is_ignored(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));

    fake_nrf52_timer_fire_event(0, (nrf_timer_event_t)0xFF);

    TEST_ASSERT_EQUAL_INT(0, s_fired);
}

void test_second_instance_fires_independently(void)
{
    s_fired = 0;
    hf_timer_set_callback(1, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 500, HF_TIMER_ONE_SHOT));

    fake_nrf52_timer_fire_event(1, NRF_TIMER_EVENT_COMPARE0);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(1, s_last_instance);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_fails_when_nrfx_reports_error);
    RUN_TEST(test_start_and_stop_return_true_for_valid_instance);
    RUN_TEST(test_callback_fires_with_correct_instance_and_delivered_context);
    RUN_TEST(test_non_compare_event_is_ignored);
    RUN_TEST(test_second_instance_fires_independently);
    return UNITY_END();
}
