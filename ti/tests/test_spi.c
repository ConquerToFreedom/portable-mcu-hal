/* Unit tests for ti/spi/hal_port_ti_mspm0_spi.c against
 * tests/fakes/ti_mspm0. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_ti_mspm0.h"

void SPI0_IRQHandler(void);

void setUp(void) { fake_ti_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;

static void on_tx(hf_spi_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_spi_t instance) { (void)instance; s_rx_fired++; }

void test_init_valid_instances(void)
{
    TEST_ASSERT_TRUE(hf_spi_init(0, 1000000, HF_SPI_MODE_0));
    TEST_ASSERT_TRUE(hf_spi_init(1, 1000000, HF_SPI_MODE_3));
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_spi_init(2, 1000000, HF_SPI_MODE_0)); /* only SPI0/1 exist */
}

void test_write_asserts_then_releases_cs(void)
{
    hf_gpio_t cs = HF_PIN(0, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_gpio_write(cs, true);

    uint8_t data[] = { 0xDE, 0xAD };
    TEST_ASSERT_TRUE(hf_spi_write(0, cs, data, sizeof(data)));
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_read_dummy_feeds_and_captures_bytes(void)
{
    hf_gpio_t cs = HF_PIN(0, 4);
    fake_ti_spi_next_rx_byte = 0x5A;
    uint8_t data[2];
    TEST_ASSERT_TRUE(hf_spi_read(0, cs, data, sizeof(data)));
    TEST_ASSERT_EQUAL_UINT8(0x5A, data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5A, data[1]);
}

void test_write_async_fires_tx_callback_and_releases_cs(void)
{
    s_tx_fired = 0;
    hf_gpio_t cs = HF_PIN(0, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_gpio_read(cs));

    fake_ti_spi_pending_interrupt = DL_SPI_IIDX_RX;
    SPI0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_read_async_fires_rx_callback(void)
{
    s_rx_fired = 0;
    hf_gpio_t cs = HF_PIN(0, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_rx_callback(0, on_rx);

    uint8_t data[1];
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data)));

    fake_ti_spi_pending_interrupt = DL_SPI_IIDX_RX;
    SPI0_IRQHandler();
    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
}

void test_busy_rejects_second_async_transfer(void)
{
    hf_gpio_t cs = HF_PIN(0, 4);
    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_spi_read_async(0, cs, data, sizeof(data)));

    fake_ti_spi_pending_interrupt = DL_SPI_IIDX_RX;
    SPI0_IRQHandler(); /* clear busy so it doesn't leak into the next test */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_asserts_then_releases_cs);
    RUN_TEST(test_read_dummy_feeds_and_captures_bytes);
    RUN_TEST(test_write_async_fires_tx_callback_and_releases_cs);
    RUN_TEST(test_read_async_fires_rx_callback);
    RUN_TEST(test_busy_rejects_second_async_transfer);
    return UNITY_END();
}
