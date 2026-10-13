/* Unit tests for stm/uart/hal_port_stm32_uart.c against tests/fakes/stm32. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;

static void on_tx(hf_uart_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_uart_t instance) { (void)instance; s_rx_fired++; }

void test_init_success_arms_background_receive(void)
{
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_uart_init(3, 115200)); /* only huart1-3 => 0-2 */
}

void test_write_blocking_sends_exact_buffer(void)
{
    uint8_t data[] = { 'h', 'i' };
    hf_uart_write(0, data, sizeof(data));
    TEST_ASSERT_EQUAL_PTR(data, fake_stm32_last_tx_data);
    TEST_ASSERT_EQUAL_UINT16(2, fake_stm32_last_tx_len);
}

void test_write_async_fires_registered_callback_on_completion(void)
{
    s_tx_fired = 0;
    uint8_t data[] = { 'x' };
    hf_uart_set_tx_callback(0, on_tx);

    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    HAL_UART_TxCpltCallback(&huart1); /* simulate the ISR firing */
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
}

void test_write_async_rejects_second_transfer_while_busy(void)
{
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_uart_write_async(0, data, sizeof(data)));
    HAL_UART_TxCpltCallback(&huart1); /* clear busy so it doesn't leak into the next test */
}

void test_error_callback_clears_busy_without_firing_callback(void)
{
    s_tx_fired = 0;
    uint8_t data[] = { 'x' };
    hf_uart_set_tx_callback(0, on_tx);
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));

    HAL_UART_ErrorCallback(&huart1);
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired); /* no false success */

    /* busy flag must have been cleared so a new transfer can start */
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
}

void test_read_drains_ring_buffer_after_rx_interrupt(void)
{
    s_rx_fired = 0;
    hf_uart_set_rx_callback(0, on_rx);
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));

    TEST_ASSERT_NOT_NULL(fake_stm32_last_rx_buf);
    *fake_stm32_last_rx_buf = 'A';
    HAL_UART_RxCpltCallback(&huart1);
    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);

    uint8_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT32(1, hf_uart_read(0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8('A', out[0]);
    /* second read finds nothing left to drain */
    TEST_ASSERT_EQUAL_UINT32(0, hf_uart_read(0, out, sizeof(out)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_success_arms_background_receive);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_blocking_sends_exact_buffer);
    RUN_TEST(test_write_async_fires_registered_callback_on_completion);
    RUN_TEST(test_write_async_rejects_second_transfer_while_busy);
    RUN_TEST(test_error_callback_clears_busy_without_firing_callback);
    RUN_TEST(test_read_drains_ring_buffer_after_rx_interrupt);
    return UNITY_END();
}
