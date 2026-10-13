/* Unit tests for silicon_labs/i2c/hal_port_silabs_slgpio_i2c.c
 * (TARGET_SILABS_SLGPIO, current sl_i2c) against
 * tests/fakes/silabs_slgpio. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs_slgpio.h"

void setUp(void) { fake_silabs_slgpio_reset(); }
void tearDown(void) {}

static int s_tx_fired, s_tx_success;
static int s_rx_fired, s_rx_success;

static void on_tx(hf_i2c_t instance, bool success) { (void)instance; s_tx_fired++; s_tx_success = success; }
static void on_rx(hf_i2c_t instance, bool success) { (void)instance; s_rx_fired++; s_rx_success = success; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    TEST_ASSERT_TRUE(hf_i2c_init(1, 1000000)); /* Fast Mode Plus */
}

void test_write_blocking_succeeds(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_TRUE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_blocking_reports_failure(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    fake_sli2c_next_status = SL_STATUS_FAIL;
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_FALSE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_read_uses_native_transfer(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    uint8_t reg = 0x0F, value;
    TEST_ASSERT_TRUE(hf_i2c_write_read(0, 0x68, &reg, 1, &value, 1));
}

void test_async_write_fires_tx_callback_only_after_event(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    fake_sli2c_auto_complete = false;
    hf_i2c_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0xAA };
    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    fake_sli2c_fire_event(SL_I2C_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(s_tx_success);
}

void test_async_read_fires_rx_callback(void)
{
    s_rx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    fake_sli2c_auto_complete = false;
    hf_i2c_set_rx_callback(0, on_rx);

    uint8_t data[4];
    TEST_ASSERT_TRUE(hf_i2c_read_async(0, 0x50, data, sizeof(data)));
    fake_sli2c_fire_event(SL_I2C_EVENT_COMPLETED);

    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
    TEST_ASSERT_TRUE(s_rx_success);
}

void test_async_nack_reports_failure(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    fake_sli2c_auto_complete = false;
    hf_i2c_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0xAA };
    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    fake_sli2c_fire_event(SL_I2C_EVENT_NACK_RECEIVED);

    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_FALSE(s_tx_success);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_write_blocking_succeeds);
    RUN_TEST(test_write_blocking_reports_failure);
    RUN_TEST(test_write_read_uses_native_transfer);
    RUN_TEST(test_async_write_fires_tx_callback_only_after_event);
    RUN_TEST(test_async_read_fires_rx_callback);
    RUN_TEST(test_async_nack_reports_failure);
    return UNITY_END();
}
