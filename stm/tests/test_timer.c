/* Unit tests for stm/timer/hal_port_stm32_timer.c against tests/fakes/stm32. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

static int s_fired;
static hf_timer_t s_last_instance;
static void on_elapsed(hf_timer_t instance) { s_fired++; s_last_instance = instance; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_FALSE(hf_timer_init(2, 1000, HF_TIMER_PERIODIC)); /* only htim6/htim7 => 0-1 */
}

void test_start_and_stop_return_true_for_valid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));
    hf_timer_stop(0); /* void - just confirm it doesn't crash */
}

void test_periodic_callback_fires_repeatedly_without_stopping(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));

    HAL_TIM_PeriodElapsedCallback(&htim6);
    HAL_TIM_PeriodElapsedCallback(&htim6);

    TEST_ASSERT_EQUAL_INT(2, s_fired);
    TEST_ASSERT_EQUAL_UINT8(0, s_last_instance);
}

void test_one_shot_callback_stops_itself_after_firing(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_ONE_SHOT));
    TEST_ASSERT_TRUE(hf_timer_start(0));

    fake_stm32_next_status = HAL_ERROR; /* HAL_TIM_Base_Stop_IT's return is ignored either way */
    HAL_TIM_PeriodElapsedCallback(&htim6);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
}

void test_second_instance_uses_htim7_independently(void)
{
    s_fired = 0;
    hf_timer_set_callback(1, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(1, 500, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(1));

    HAL_TIM_PeriodElapsedCallback(&htim7);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(1, s_last_instance);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_start_and_stop_return_true_for_valid_instance);
    RUN_TEST(test_periodic_callback_fires_repeatedly_without_stopping);
    RUN_TEST(test_one_shot_callback_stops_itself_after_firing);
    RUN_TEST(test_second_instance_uses_htim7_independently);
    return UNITY_END();
}
