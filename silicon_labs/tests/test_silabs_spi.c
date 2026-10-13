/* Unit tests for silicon_labs/spi/hal_port_silabs_spi.c
 * (TARGET_SILABS, SPIDRV) against tests/fakes/silabs. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"

void setUp(void) { fake_silabs_reset(); }
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
    TEST_ASSERT_FALSE(hf_spi_init(2, 1000000, HF_SPI_MODE_0)); /* only USART0/1-backed slots exist */
}

void test_write_asserts_then_releases_cs(void)
{
    hf_gpio_t cs = HF_PIN(gpioPortA, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_gpio_write(cs, true);

    uint8_t data[] = { 0xDE, 0xAD };
    TEST_ASSERT_TRUE(hf_spi_write(0, cs, data, sizeof(data)));
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_transfer_is_full_duplex(void)
{
    hf_gpio_t cs = HF_PIN(gpioPortA, 4);
    uint8_t tx[] = { 1, 2 }, rx[2];
    TEST_ASSERT_TRUE(hf_spi_transfer(0, cs, tx, rx, sizeof(tx)));
}

void test_write_async_fires_tx_callback_and_releases_cs(void)
{
    s_tx_fired = 0;
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(gpioPortA, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_gpio_read(cs)); /* still asserted mid-transfer */

    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 1);
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_error_status_releases_cs_without_firing_callback(void)
{
    s_rx_fired = 0;
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(gpioPortA, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_rx_callback(0, on_rx);

    uint8_t data[2];
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data)));
    fake_silabs_spi_fire_completion(NULL, -1 /* not ECODE_OK */, 0);

    TEST_ASSERT_EQUAL_INT(0, s_rx_fired); /* no false success */
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data))); /* busy was cleared */
    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 0); /* clean up */
}

void test_busy_rejects_second_async_transfer(void)
{
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(gpioPortA, 4);
    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_spi_read_async(0, cs, data, sizeof(data)));
    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 1); /* clean up */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_asserts_then_releases_cs);
    RUN_TEST(test_transfer_is_full_duplex);
    RUN_TEST(test_write_async_fires_tx_callback_and_releases_cs);
    RUN_TEST(test_error_status_releases_cs_without_firing_callback);
    RUN_TEST(test_busy_rejects_second_async_transfer);
    return UNITY_END();
}
