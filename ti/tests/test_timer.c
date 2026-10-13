/* Unit tests for ti/timer/hal_port_ti_mspm0_timer.c against tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void TIMG0_IRQHandler(void);
void TIMG1_IRQHandler(void);

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

static int s_fired;
static hf_timer_t s_last_instance;
static void on_elapsed(hf_timer_t instance) { s_fired++; s_last_instance = instance; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_FALSE(hf_timer_init(2, 1000, HF_TIMER_PERIODIC)); /* only TIMG0/TIMG1 => 0-1 */
}

void test_start_and_stop_return_true_for_valid_instance(void)
{
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));
    hf_timer_stop(0); /* void - just confirm it doesn't crash */
}

void test_callback_fires_via_irq_handler(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_start(0));

    fake_ti_timer_pending_interrupt = DL_TIMER_IIDX_ZERO;
    TIMG0_IRQHandler();

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(0, s_last_instance);
}

void test_no_callback_when_interrupt_not_pending(void)
{
    s_fired = 0;
    hf_timer_set_callback(0, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));

    fake_ti_timer_pending_interrupt = DL_TIMER_IIDX_NONE;
    TIMG0_IRQHandler();

    TEST_ASSERT_EQUAL_INT(0, s_fired);
}

void test_second_instance_fires_via_its_own_irq_handler(void)
{
    s_fired = 0;
    hf_timer_set_callback(1, on_elapsed);
    TEST_ASSERT_TRUE(hf_timer_init(0, 1000, HF_TIMER_PERIODIC));
    TEST_ASSERT_TRUE(hf_timer_init(1, 500, HF_TIMER_ONE_SHOT));

    fake_ti_timer_pending_interrupt = DL_TIMER_IIDX_ZERO;
    TIMG1_IRQHandler();

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT8(1, s_last_instance);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_start_and_stop_return_true_for_valid_instance);
    RUN_TEST(test_callback_fires_via_irq_handler);
    RUN_TEST(test_no_callback_when_interrupt_not_pending);
    RUN_TEST(test_second_instance_fires_via_its_own_irq_handler);
    return UNITY_END();
}
