/* Unit tests for silicon_labs/uart/hal_port_silabs_slgpio_uart.c
 * (TARGET_SILABS_SLGPIO) against tests/fakes/silabs. This adapter
 * talks to the same classic emlib USART hardware as
 * hal_port_silabs_uart.c (TARGET_SILABS) - see that adapter's own
 * header comment for why - so it reuses fake_silabs.c's fakes rather
 * than anything slgpio-specific. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"

void USART0_TX_IRQHandler(void);
void USART0_RX_IRQHandler(void);

void setUp(void) { fake_silabs_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;

static void on_tx(hf_uart_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_uart_t instance) { (void)instance; s_rx_fired++; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_uart_init(0, 9600));
    TEST_ASSERT_TRUE(hf_uart_init(1, 9600));
}

void test_write_blocking_sends_bytes(void)
{
    uint8_t data[] = { 'h', 'i' };
    hf_uart_write(0, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32('i', fake_silabs_usart0.TXDATA);
}

void test_write_async_completes_over_txbl_then_txc(void)
{
    s_tx_fired = 0;
    hf_uart_set_tx_callback(0, on_tx);
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));

    fake_silabs_usart_set_flags(0, USART_IF_TXBL);
    USART0_TX_IRQHandler();
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    fake_silabs_usart_set_flags(0, USART_IF_TXC);
    USART0_TX_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
}

void test_write_async_rejects_second_transfer_while_busy(void)
{
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_uart_write_async(0, data, sizeof(data)));

    fake_silabs_usart_set_flags(0, USART_IF_TXBL);
    USART0_TX_IRQHandler();
    fake_silabs_usart_set_flags(0, USART_IF_TXC);
    USART0_TX_IRQHandler();
}

void test_read_drains_ring_after_rx_irq(void)
{
    s_rx_fired = 0;
    hf_uart_set_rx_callback(0, on_rx);
    fake_silabs_usart0.RXDATA = 'A';
    fake_silabs_usart_set_flags(0, USART_IF_RXDATAV);
    USART0_RX_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);

    uint8_t out[4] = {0};
    TEST_ASSERT_EQUAL_UINT32(1, hf_uart_read(0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8('A', out[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_write_blocking_sends_bytes);
    RUN_TEST(test_write_async_completes_over_txbl_then_txc);
    RUN_TEST(test_write_async_rejects_second_transfer_while_busy);
    RUN_TEST(test_read_drains_ring_after_rx_irq);
    return UNITY_END();
}
