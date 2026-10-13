/* Unit tests for silicon_labs/timer/hal_port_silabs_slgpio_timer.c
 * (TARGET_SILABS_SLGPIO, sl_sleeptimer) against tests/fakes/silabs_slgpio. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs_slgpio.h"

void setUp(void) { fake_silabs_slgpio_reset(); }
void tearDown(void) {}

static int s_fired;
static hf_timer_t s_last_instance;
static void on_elapsed(hf_timer_t instance) { s_fired++; s_last_instance = instance; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_FALSE(hf_timer_init(2, 1000, HF_TIMER_PERIODIC)); /* software-imposed count == 2 */
}

void test_init_calls_sl_sleeptimer_init(void)
{
    TEST_ASSERT_FALSE(fake_sl_sleeptimer_is_initialized());
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(fake_sl_sleeptimer_is_initialized());
}

void test_start_returns_true_and_stop_does_not_crash(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));
    hf_timer_stop(0);
}

void test_callback_fires_with_correct_instance(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));

    fake_sl_sleeptimer_fire(0);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(0, s_last_instance);
}

void test_period_under_1ms_rounds_up_to_1ms(void)
{
    /* Not directly observable from outside (s_ticks is adapter-private),
     * but a period_us of 1 must not be rejected - the rounding-up
     * behavior is what keeps sl_sleeptimer_ms32_to_tick() from ever
     * seeing a 0ms request. */
    TEST_ASSERT_TRUE(hf_timer_init(0, 1, HF_TIMER_ONE_SHOT));
    TEST_ASSERT_TRUE(hf_timer_start(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_calls_sl_sleeptimer_init);
    RUN_TEST(test_start_returns_true_and_stop_does_not_crash);
    RUN_TEST(test_callback_fires_with_correct_instance);
    RUN_TEST(test_period_under_1ms_rounds_up_to_1ms);
    return UNITY_END();
}
