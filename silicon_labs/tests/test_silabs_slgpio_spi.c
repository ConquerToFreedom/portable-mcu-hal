/* Unit tests for silicon_labs/spi/hal_port_silabs_slgpio_spi.c
 * (TARGET_SILABS_SLGPIO, SPIDRV) against tests/fakes/silabs. This
 * adapter is functionally identical to hal_port_silabs_spi.c
 * (TARGET_SILABS) - SPIDRV is shared unchanged between both targets -
 * so it reuses fake_silabs.c's fakes. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_silabs.h"
#include "fake_silabs_slgpio.h" /* only for SL_GPIO_PORT_A - CS is a plain hf_gpio_t pin */

void setUp(void) { fake_silabs_reset(); fake_silabs_slgpio_reset(); }
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
    TEST_ASSERT_FALSE(hf_spi_init(2, 1000000, HF_SPI_MODE_0));
}

void test_write_asserts_then_releases_cs(void)
{
    hf_gpio_t cs = HF_PIN(SL_GPIO_PORT_A, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_gpio_write(cs, true);

    uint8_t data[] = { 0xDE, 0xAD };
    TEST_ASSERT_TRUE(hf_spi_write(0, cs, data, sizeof(data)));
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_transfer_is_full_duplex(void)
{
    hf_gpio_t cs = HF_PIN(SL_GPIO_PORT_A, 4);
    uint8_t tx[] = { 1, 2 }, rx[2];
    TEST_ASSERT_TRUE(hf_spi_transfer(0, cs, tx, rx, sizeof(tx)));
}

void test_write_async_fires_tx_callback_and_releases_cs(void)
{
    s_tx_fired = 0;
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(SL_GPIO_PORT_A, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_gpio_read(cs));

    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 1);
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
}

void test_read_async_fires_rx_callback(void)
{
    s_rx_fired = 0;
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(SL_GPIO_PORT_A, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_rx_callback(0, on_rx);

    uint8_t data[2];
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data)));
    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 2);

    TEST_ASSERT_EQUAL_INT(1, s_rx_fired);
}

void test_busy_rejects_second_async_transfer(void)
{
    fake_silabs_spi_auto_complete = false;
    hf_gpio_t cs = HF_PIN(SL_GPIO_PORT_A, 4);
    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_spi_read_async(0, cs, data, sizeof(data)));
    fake_silabs_spi_fire_completion(NULL, ECODE_OK, 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_valid_instances);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_asserts_then_releases_cs);
    RUN_TEST(test_transfer_is_full_duplex);
    RUN_TEST(test_write_async_fires_tx_callback_and_releases_cs);
    RUN_TEST(test_read_async_fires_rx_callback);
    RUN_TEST(test_busy_rejects_second_async_transfer);
    return UNITY_END();
}
