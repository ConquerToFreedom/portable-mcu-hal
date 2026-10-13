/* Unit tests for ti/i2c/hal_port_ti_mspm0_i2c.c against
 * tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void I2C0_IRQHandler(void);

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

static int s_tx_fired, s_tx_success;
static int s_rx_fired, s_rx_success;

static void on_tx(hf_i2c_t instance, bool success) { (void)instance; s_tx_fired++; s_tx_success = success; }
static void on_rx(hf_i2c_t instance, bool success) { (void)instance; s_rx_fired++; s_rx_success = success; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 100000));
    TEST_ASSERT_TRUE(hf_i2c_init(1, 400000));
}

void test_write_blocking_succeeds(void)
{
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_TRUE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_blocking_reports_bus_error(void)
{
    fake_ti_i2c_status = DL_I2C_CONTROLLER_STATUS_IDLE | DL_I2C_CONTROLLER_STATUS_ERROR;
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_FALSE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_read_blocking_returns_configured_byte(void)
{
    fake_ti_i2c_next_rx_byte = 0xAB;
    uint8_t value = 0;
    TEST_ASSERT_TRUE(hf_i2c_read(0, 0x50, &value, 1));
    TEST_ASSERT_EQUAL_UINT8(0xAB, value);
}

void test_write_read_uses_repeated_start(void)
{
    uint8_t reg = 0x0F, value;
    TEST_ASSERT_TRUE(hf_i2c_write_read(0, 0x68, &reg, 1, &value, 1));
}

void test_async_write_fires_tx_callback_only_after_irq(void)
{
    s_tx_fired = 0;
    hf_i2c_set_tx_callback(0, on_tx);
    uint8_t data[] = { 0xAA };
    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    fake_ti_i2c_pending_interrupt = DL_I2C_IIDX_CONTROLLER_TX_DONE;
    I2C0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(s_tx_success);
}

void test_async_read_fires_rx_callback(void)
{
    s_rx_fired = 0;
    hf_i2c_set_rx_callback(0, on_rx);
    uint8_t data[1];
    TEST_ASSERT_TRUE(hf_i2c_read_async(0, 0x50, data, sizeof(data)));

    fake_ti_i2c_next_rx_byte = 0x77;
    fake_ti_i2c_pending_interrupt = DL_I2C_IIDX_CONTROLLER_RX_DONE;
    I2C0_IRQHandler();

    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
    TEST_ASSERT_TRUE(s_rx_success);
    TEST_ASSERT_EQUAL_UINT8(0x77, data[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_write_blocking_succeeds);
    RUN_TEST(test_write_blocking_reports_bus_error);
    RUN_TEST(test_read_blocking_returns_configured_byte);
    RUN_TEST(test_write_read_uses_repeated_start);
    RUN_TEST(test_async_write_fires_tx_callback_only_after_irq);
    RUN_TEST(test_async_read_fires_rx_callback);
    return UNITY_END();
}
