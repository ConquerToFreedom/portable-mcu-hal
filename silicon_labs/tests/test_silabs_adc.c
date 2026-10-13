/* Unit tests for silicon_labs/adc/hal_port_silabs_adc.c
 * (TARGET_SILABS, classic emlib ADC) against tests/fakes/silabs. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"

void ADC0_IRQHandler(void);

void setUp(void) { fake_silabs_reset(); }
void tearDown(void) {}

static int s_fired;
static void on_done(hf_adc_t instance) { (void)instance; s_fired++; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_adc_init(0));
    TEST_ASSERT_TRUE(hf_adc_init(1));
    TEST_ASSERT_FALSE(hf_adc_init(2)); /* only ADC0/ADC1 exist */
}

void test_read_blocking_returns_configured_value(void)
{
    uint16_t value = 0;
    fake_silabs_adc_next_value = 2048;
    TEST_ASSERT_TRUE(hf_adc_read(0, 3, &value));
    TEST_ASSERT_EQUAL_UINT16(2048, value);
}

void test_read_invalid_channel_fails(void)
{
    uint16_t value;
    TEST_ASSERT_FALSE(hf_adc_read(0, 9, &value)); /* only channels 0-7 exist */
}

void test_read_async_fires_callback_via_irq(void)
{
    s_fired = 0;
    uint16_t value = 0;
    hf_adc_set_callback(0, on_done);

    TEST_ASSERT_TRUE(hf_adc_read_async(0, 5, &value));
    TEST_ASSERT_EQUAL_INT(0, s_fired);

    fake_silabs_adc_next_value = 1234;
    ADC0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT16(1234, value);
}

void test_busy_rejects_second_async_read(void)
{
    uint16_t value;
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));
    TEST_ASSERT_FALSE(hf_adc_read_async(0, 0, &value));
    ADC0_IRQHandler(); /* clear busy so it doesn't leak into the next test */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_read_blocking_returns_configured_value);
    RUN_TEST(test_read_invalid_channel_fails);
    RUN_TEST(test_read_async_fires_callback_via_irq);
    RUN_TEST(test_busy_rejects_second_async_read);
    return UNITY_END();
}
