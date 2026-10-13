/* Unit tests for nrf/uart/hal_port_nrf52_uart.c against
 * tests/fakes/nrf52.
 *
 * Only instance 0 is exercised for async completion - see the "REAL
 * FINDING" comment in tests/fakes/nrf52/nrfx_uarte.h: hf_uart_init()
 * calls nrfx_uarte_init() without an explicit context argument, unlike
 * this vendor's I2C/SPI adapters, so this fake always invokes the
 * event handler with p_context == NULL, same as what the real 3-arg
 * call would produce absent some other context-delivery mechanism this
 * pass didn't confirm.
 */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_nrf52.h"

void setUp(void) { fake_nrf52_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;

static void on_tx(hf_uart_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_uart_t instance) { (void)instance; s_rx_fired++; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
    TEST_ASSERT_TRUE(hf_uart_init(1, 9600));
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_uart_init(2, 115200)); /* only UARTE0/1 exist */
}

void test_write_blocking_completes_via_auto_complete(void)
{
    /* fake_nrf52_auto_complete defaults true, so the adapter's internal
     * spin-wait terminates immediately - if this hangs, the test
     * harness's own timeout is what will report the failure. */
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
    uint8_t data[] = { 'h', 'i' };
    hf_uart_write(0, data, sizeof(data));
    TEST_PASS();
}

void test_write_async_fires_callback_only_after_event_fires(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
    fake_nrf52_auto_complete = false;
    hf_uart_set_tx_callback(0, on_tx);

    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired);

    nrfx_uarte_event_t event;
    event.type = NRFX_UARTE_EVT_TX_DONE;
    fake_nrf52_uarte_fire_event(0, &event);
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
}

void test_write_async_rejects_second_transfer_while_busy(void)
{
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
    fake_nrf52_auto_complete = false;
    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_uart_write_async(0, data, sizeof(data)));

    nrfx_uarte_event_t event;
    event.type = NRFX_UARTE_EVT_TX_DONE;
    fake_nrf52_uarte_fire_event(0, &event); /* clear busy for the next test */
}

void test_error_event_clears_busy_without_firing_callback(void)
{
    s_tx_fired = 0;
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));
    fake_nrf52_auto_complete = false;
    hf_uart_set_tx_callback(0, on_tx);

    uint8_t data[] = { 'x' };
    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data)));

    nrfx_uarte_event_t event;
    event.type = NRFX_UARTE_EVT_ERROR;
    fake_nrf52_uarte_fire_event(0, &event);
    TEST_ASSERT_EQUAL_INT(0, s_tx_fired); /* no false success */

    TEST_ASSERT_TRUE(hf_uart_write_async(0, data, sizeof(data))); /* busy was cleared */
    fake_nrf52_uarte_fire_event(0, &event); /* clean up */
}

void test_read_drains_after_rx_done_event(void)
{
    s_rx_fired = 0;
    hf_uart_set_rx_callback(0, on_rx);
    TEST_ASSERT_TRUE(hf_uart_init(0, 115200));

    uint8_t byte = 'A';
    nrfx_uarte_event_t event;
    event.type = NRFX_UARTE_EVT_RX_DONE;
    event.data.rx.p_buffer = &byte;
    event.data.rx.length = 1;
    fake_nrf52_uarte_fire_event(0, &event);

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
    RUN_TEST(test_write_blocking_completes_via_auto_complete);
    RUN_TEST(test_write_async_fires_callback_only_after_event_fires);
    RUN_TEST(test_write_async_rejects_second_transfer_while_busy);
    RUN_TEST(test_error_event_clears_busy_without_firing_callback);
    RUN_TEST(test_read_drains_after_rx_done_event);
    return UNITY_END();
}
