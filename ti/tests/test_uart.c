/* Unit tests for ti/uart/hal_port_ti_mspm0_uart.c against
 * tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void UART0_IRQHandler(void);

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;

static void on_tx(hf_uart_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_uart_t instance) { (void)instance; s_rx_fired++; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_uart_init(0, 9600));
    TEST_ASSERT_TRUE(hf_uart_init(1, 115200));
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_uart_init(2, 9600)); /* only UART0/1 exist */
}

void test_write_blocking_sends_bytes(void)
{
    uint8_t data[] = { 'h', 'i' };
    hf_uart_write(0, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT8('i', fake_ti_uart_last_tx_byte);
}

void test_write_async_completes_via_eot_done_irq(void)
{
    s_tx_fired = 0;
    hf_uart_set_tx_callback(0, on_tx);
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    fake_ti_uart_pending_interrupt = DL_UART_MAIN_IIDX_EOT_DONE;
    UART0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
}

void test_write_async_rejects_second_transfer_while_busy(void)
{
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_uart_write_async(0, data, sizeof(data)));

    fake_ti_uart_pending_interrupt = DL_UART_MAIN_IIDX_EOT_DONE;
    UART0_IRQHandler(); /* clear busy so it doesn't leak into the next test */
}

void test_read_drains_ring_after_rx_irq(void)
{
    s_rx_fired = 0;
    hf_uart_set_rx_callback(0, on_rx);
    fake_ti_uart_rx_fifo_empty = false;
    fake_ti_uart_next_rx_byte = 'A';
    fake_ti_uart_pending_interrupt = DL_UART_MAIN_IIDX_RX;
    UART0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);

    uint8_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT32(1, hf_uart_read(0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8('A', out[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_blocking_sends_bytes);
    RUN_TEST(test_write_async_completes_via_eot_done_irq);
    RUN_TEST(test_write_async_rejects_second_transfer_while_busy);
    RUN_TEST(test_read_drains_ring_after_rx_irq);
    return UNITY_END();
}
