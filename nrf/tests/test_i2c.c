/* Unit tests for nrf/i2c/hal_port_nrf52_i2c.c against tests/fakes/nrf52. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

static int s_tx_fired, s_tx_success;
static int s_rx_fired, s_rx_success;

static void on_tx(hf_i2c_t instance, bool success) { (void)instance; s_tx_fired++; s_tx_success = success; }
static void on_rx(hf_i2c_t instance, bool success) { (void)instance; s_rx_fired++; s_rx_success = success; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    TEST_ASSERT_TRUE(hf_i2c_init(1, 100000));
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_i2c_init(2, 400000)); /* only TWIM0/1 exist */
}

void test_write_blocking_succeeds(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_TRUE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_blocking_reports_nack(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    fake_nrf52_twim_auto_success = false; /* the only way to fail a blocking call - see fake_nrf52.h */
    uint8_t data[] = { 0x01 };
    TEST_ASSERT_FALSE(hf_i2c_write(0, 0x50, data, sizeof(data)));
}

void test_write_read_uses_txrx_descriptor(void)
{
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    uint8_t reg = 0x0F, value;
    TEST_ASSERT_TRUE(hf_i2c_write_read(0, 0x68, &reg, 1, &value, 1));
}

void test_async_write_fires_tx_callback_only_after_event(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    fake_nrf52_auto_complete = false;
    hf_i2c_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0xAA };
    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    nrfx_twim_event_t event;
    event.type = NRFX_TWIM_EVT_DONE;
    event.xfer_desc.type = NRFX_TWIM_XFER_TX;
    fake_nrf52_twim_fire_event(0, &event);

    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(s_tx_success);
}

void test_async_read_fires_rx_callback(void)
{
    s_rx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    fake_nrf52_auto_complete = false;
    hf_i2c_set_rx_callback(0, on_rx);

    uint8_t data[4];
    TEST_ASSERT_TRUE(hf_i2c_read_async(0, 0x50, data, sizeof(data)));

    nrfx_twim_event_t event;
    event.type = NRFX_TWIM_EVT_DONE;
    event.xfer_desc.type = NRFX_TWIM_XFER_RX;
    fake_nrf52_twim_fire_event(0, &event);

    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
    TEST_ASSERT_TRUE(s_rx_success);
}

void test_async_nack_reports_failure(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_i2c_init(0, 400000));
    fake_nrf52_auto_complete = false;
    hf_i2c_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0xAA };
    TEST_ASSERT_TRUE(hf_i2c_write_async(0, 0x50, data, sizeof(data)));

    nrfx_twim_event_t event;
    event.type = NRFX_TWIM_EVT_ADDRESS_NACK;
    event.xfer_desc.type = NRFX_TWIM_XFER_TX;
    fake_nrf52_twim_fire_event(0, &event);

    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_FALSE(s_tx_success);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_blocking_succeeds);
    RUN_TEST(test_write_blocking_reports_nack);
    RUN_TEST(test_write_read_uses_txrx_descriptor);
    RUN_TEST(test_async_write_fires_tx_callback_only_after_event);
    RUN_TEST(test_async_read_fires_rx_callback);
    RUN_TEST(test_async_nack_reports_failure);
    return UNITY_END();
}
