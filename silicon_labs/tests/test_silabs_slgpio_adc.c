/* Unit tests for silicon_labs/adc/hal_port_silabs_slgpio_adc.c
 * (TARGET_SILABS_SLGPIO, IADC) against tests/fakes/silabs_slgpio. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs_slgpio.h"

void IADC0_IRQHandler(void);

void setUp(void) { fake_silabs_slgpio_reset(); }
void tearDown(void) {}

static int s_fired;
static void on_done(hf_adc_t instance) { (void)instance; s_fired++; }

void test_init_valid_and_invalid_instance(void)
{
    TEST_ASSERT_TRUE(hf_adc_init(0));
    TEST_ASSERT_FALSE(hf_adc_init(1)); /* only one IADC peripheral exists */
}

void test_read_blocking_masks_to_12_bits(void)
{
    uint16_t value = 0;
    fake_iadc_next_value = 0xFFFF; /* raw register value wider than 12 bits */
    TEST_ASSERT_TRUE(hf_adc_read(0, 3, &value));
    TEST_ASSERT_EQUAL_UINT16(0x0FFF, value);
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

    fake_iadc_next_value = 1234;
    IADC0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_fired);
    TEST_ASSERT_EQUAL_UINT16(1234, value);
}

void test_busy_rejects_second_async_read(void)
{
    uint16_t value;
    TEST_ASSERT_TRUE(hf_adc_read_async(0, 0, &value));
    TEST_ASSERT_FALSE(hf_adc_read_async(0, 0, &value));
    IADC0_IRQHandler(); /* clear busy so it doesn't leak into the next test */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_and_invalid_instance);
    RUN_TEST(test_read_blocking_masks_to_12_bits);
    RUN_TEST(test_read_invalid_channel_fails);
    RUN_TEST(test_read_async_fires_callback_via_irq);
    RUN_TEST(test_busy_rejects_second_async_read);
    return UNITY_END();
}
