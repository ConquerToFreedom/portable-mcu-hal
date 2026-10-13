/* Unit tests for stm/adc/hal_port_stm32_adc.c against tests/fakes/stm32. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

static int s_fired;
static void on_done(hf_adc_t instance) { (void)instance; s_fired++; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_adc_init(0));
    TEST_ASSERT_FALSE(hf_adc_init(3)); /* only hadc1-3 => 0-2 */
}

void test_read_blocking_returns_configured_value(void)
{
    uint16_t value = 0;
    fake_stm32_adc_next_value = 2048;
    TEST_ASSERT_TRUE(hf_adc_read(0, 3, &value));
    TEST_ASSERT_EQUAL_UINT16(2048, value);
}

void test_read_blocking_fails_on_conversion_error(void)
{
    uint16_t value = 0;
    fake_stm32_next_status = HAL_ERROR;
    TEST_ASSERT_FALSE(hf_adc_read(0, 3, &value));
}

void test_read_async_fires_callback_with_value(void)
{
    s_fired = 0;
    uint16_t value = 0;
    hf_adc_set_callback(0, on_done);

    TEST_ASSERT_TRUE(hf_adc_read_async(0, 5, &value));
    fake_stm32_adc_next_value = 1234;
    HAL_ADC_ConvCpltCallback(&hadc1);

    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT16(1234, value);
}

void test_busy_rejects_second_async_read(void)
{
    uint16_t value;
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));
    TEST_ASSERT_FALSE(hf_adc_read_async(0, 0, &value));
    HAL_ADC_ConvCpltCallback(&hadc1); /* clear busy so it doesn't leak into the next test */
}

void test_error_callback_clears_busy_without_firing_callback(void)
{
    s_fired = 0;
    uint16_t value;
    hf_adc_set_callback(0, on_done);
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));

    HAL_ADC_ErrorCallback(&hadc1);
    TEST_ASSERT_EQUAL_INT(0, s_fired); /* no false success */

    /* busy flag cleared so a new conversion can start */
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_read_blocking_returns_configured_value);
    RUN_TEST(test_read_blocking_fails_on_conversion_error);
    RUN_TEST(test_read_async_fires_callback_with_value);
    RUN_TEST(test_busy_rejects_second_async_read);
    RUN_TEST(test_error_callback_clears_busy_without_firing_callback);
    return UNITY_END();
}
