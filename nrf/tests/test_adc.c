/* Unit tests for nrf/adc/hal_port_nrf52_adc.c against tests/fakes/nrf52. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

static int s_fired;
static void on_done(hf_adc_t instance) { (void)instance; s_fired++; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_adc_init(0));
    TEST_ASSERT_FALSE(hf_adc_init(1)); /* only one SAADC peripheral exists */
}

void test_init_is_idempotent(void)
{
    TEST_ASSERT_TRUE(hf_adc_init(0));
    TEST_ASSERT_TRUE(hf_adc_init(0));
}

void test_read_blocking_returns_configured_value(void)
{
    uint16_t value = 0;
    fake_nrf52_saadc_next_blocking_value = 2048;
    TEST_ASSERT_TRUE(hf_adc_read(0, 3, &value));
    TEST_ASSERT_EQUAL_UINT16(2048, value);
}

void test_read_async_fires_callback_with_value(void)
{
    s_fired = 0;
    uint16_t value = 0;
    hf_adc_set_callback(0, on_done);

    TEST_ASSERT_TRUE(hf_adc_read_async(0, 5, &value));
    TEST_ASSERT_EQUAL_INT(0, s_fired); /* nothing fires until the fake says so */

    fake_nrf52_saadc_fire_done(1234);
    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT16(1234, value);
}

void test_busy_rejects_second_async_read(void)
{
    uint16_t value;
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));
    TEST_ASSERT_FALSE(hf_adc_read_async(0, 0, &value));
    fake_nrf52_saadc_fire_done(0); /* clear busy for the next test */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_init_is_idempotent);
    RUN_TEST(test_read_blocking_returns_configured_value);
    RUN_TEST(test_read_async_fires_callback_with_value);
    RUN_TEST(test_busy_rejects_second_async_read);
    return UNITY_END();
}
