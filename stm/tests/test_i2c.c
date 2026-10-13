/* Unit tests for stm/i2c/hal_port_stm32_i2c.c against tests/fakes/stm32. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

static int s_tx_fired, s_tx_success;
static int s_rx_fired, s_rx_success;

static void on_tx(hf_i2c_t instance, bool success) { (void)instance; s_tx_fired++; s_tx_success = success; }
static void on_rx(hf_i2c_t instance, bool success) { (void)instance; s_rx_fired++; s_rx_success = success; }

void test_init_ignores_clock_hz_and_just_validates_instance(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    TEST_ASSERT_FALSE(hf_i2c_init(3, 400000)); /* only hi2c1-3 => 0-2 */
}

void test_write_blocking_success(void)
{
    uint8_t data[] = { 0x01, 0x02 };
    TEST_ASSERT_TRUE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_blocking_reports_bus_failure(void)
{
    uint8_t data[] = { 0x01 };
    fake_stm32_next_status = HAL_ERROR;
    TEST_ASSERT_FALSE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_read_uses_two_seq_frames(void)
{
    uint8_t reg = 0x0F, value;
    TEST_ASSERT_TRUE(hf_i2c_write_read(0, 0x68, &reg, 1, &value, 1));
}

void test_write_read_fails_if_first_phase_fails(void)
{
    uint8_t reg = 0x0F, value;
    fake_stm32_next_status = HAL_ERROR;
    TEST_ASSERT_FALSE(hf_i2c_write_read(0, 0x68, &reg, 1, &value, 1));
}

void test_async_write_fires_tx_callback_with_success(void)
{
    s_tx_fired = 0;
    uint8_t data[] = { 0xAA };
    hf_i2c_set_tx_callback(0, on_tx);

    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    HAL_I2C_MasterTxCpltCallback(&hi2c1);

    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(s_tx_success);
}

void test_async_rejects_while_bus_busy(void)
{
    uint8_t data[] = { 0xAA };
    fake_stm32_set_i2c_state(HAL_I2C_STATE_BUSY);
    TEST_ASSERT_FALSE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_i2c_read_async(0, 0x50, data, sizeof(data)));
}

void test_error_callback_reports_failure_on_both_registered_callbacks(void)
{
    s_tx_fired = s_rx_fired = 0;
    hf_i2c_set_tx_callback(0, on_tx);
    hf_i2c_set_rx_callback(0, on_rx);

    HAL_I2C_ErrorCallback(&hi2c1);

    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_FALSE(s_tx_success);
    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
    TEST_ASSERT_FALSE(s_rx_success);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_ignores_clock_hz_and_just_validates_instance);
    RUN_TEST(test_write_blocking_success);
    RUN_TEST(test_write_blocking_reports_bus_failure);
    RUN_TEST(test_write_read_uses_two_seq_frames);
    RUN_TEST(test_write_read_fails_if_first_phase_fails);
    RUN_TEST(test_async_write_fires_tx_callback_with_success);
    RUN_TEST(test_async_rejects_while_bus_busy);
    RUN_TEST(test_error_callback_reports_failure_on_both_registered_callbacks);
    return UNITY_END();
}
