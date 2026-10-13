/* Unit tests for stm/spi/hal_port_stm32_spi.c against tests/fakes/stm32. */
#include "unity.h"
#include "hal_wrapper.h"
#include "fake_stm32_hal.h"

void setUp(void) { fake_stm32_reset(); }
void tearDown(void) {}

static int s_tx_fired;
static int s_rx_fired;
static hf_gpio_t s_cs;

static void on_tx(hf_spi_t instance) { (void)instance; s_tx_fired++; }
static void on_rx(hf_spi_t instance) { (void)instance; s_rx_fired++; }

void test_init_applies_cpol_cpha_for_each_mode(void)
{
    TEST_ASSERT_TRUE(hf_spi_init(0, 1000000, HF_SPI_MODE_0));
    TEST_ASSERT_EQUAL_UINT32(SPI_POLARITY_LOW, hspi1.Init.CLKPolarity);
    TEST_ASSERT_EQUAL_UINT32(SPI_PHASE_1EDGE, hspi1.Init.CLKPhase);

    TEST_ASSERT_TRUE(hf_spi_init(0, 1000000, HF_SPI_MODE_3));
    TEST_ASSERT_EQUAL_UINT32(SPI_POLARITY_HIGH, hspi1.Init.CLKPolarity);
    TEST_ASSERT_EQUAL_UINT32(SPI_PHASE_2EDGE, hspi1.Init.CLKPhase);
}

void test_init_invalid_instance_fails(void)
{
    TEST_ASSERT_FALSE(hf_spi_init(3, 1000000, HF_SPI_MODE_0)); /* only hspi1-3 => 0-2 */
}

void test_write_asserts_then_releases_cs(void)
{
    s_cs = HF_PIN(0, 4);
    hf_gpio_init(s_cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_gpio_write(s_cs, true);

    uint8_t data[] = { 0xDE, 0xAD };
    TEST_ASSERT_TRUE(hf_spi_write(0, s_cs, data, sizeof(data)));
    TEST_ASSERT_EQUAL_PTR(data, fake_stm32_last_tx_data);
    TEST_ASSERT_TRUE(hf_gpio_read(s_cs)); /* released (driven high) afterward */
}

void test_transfer_is_full_duplex(void)
{
    hf_gpio_t cs = HF_PIN(0, 4);
    uint8_t tx[] = { 1, 2 }, rx[2];
    TEST_ASSERT_TRUE(hf_spi_transfer(0, cs, tx, rx, sizeof(tx)));
    TEST_ASSERT_EQUAL_PTR(tx, fake_stm32_last_tx_data);
}

void test_write_async_fires_tx_callback_and_releases_cs(void)
{
    s_tx_fired = 0;
    hf_gpio_t cs = HF_PIN(0, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_tx_callback(0, on_tx);

    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_gpio_read(cs)); /* still asserted mid-transfer */

    HAL_SPI_TxCpltCallback(&hspi1);
    TEST_ASSERT_EQUAL_INT(1, s_tx_fired);
    TEST_ASSERT_TRUE(hf_gpio_read(cs)); /* released on completion */
}

void test_busy_rejects_second_async_transfer(void)
{
    hf_gpio_t cs = HF_PIN(0, 4);
    uint8_t data[] = { 0x11 };
    TEST_ASSERT_TRUE(hf_spi_write_async(0, cs, data, sizeof(data)));
    TEST_ASSERT_FALSE(hf_spi_read_async(0, cs, data, sizeof(data)));
    HAL_SPI_TxCpltCallback(&hspi1); /* clear busy so it doesn't leak into the next test */
}

void test_error_callback_releases_cs_without_firing_callback(void)
{
    s_rx_fired = 0;
    hf_gpio_t cs = HF_PIN(0, 4);
    hf_gpio_init(cs, HF_GPIO_OUTPUT_PUSHPULL);
    hf_spi_set_rx_callback(0, on_rx);

    uint8_t data[4];
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data)));
    HAL_SPI_ErrorCallback(&hspi1);

    TEST_ASSERT_EQUAL_INT(0, s_rx_fired); /* no false success */
    TEST_ASSERT_TRUE(hf_gpio_read(cs));
    /* busy flag cleared so a new transfer can start */
    TEST_ASSERT_TRUE(hf_spi_read_async(0, cs, data, sizeof(data)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_applies_cpol_cpha_for_each_mode);
    RUN_TEST(test_init_invalid_instance_fails);
    RUN_TEST(test_write_asserts_then_releases_cs);
    RUN_TEST(test_transfer_is_full_duplex);
    RUN_TEST(test_write_async_fires_tx_callback_and_releases_cs);
    RUN_TEST(test_busy_rejects_second_async_transfer);
    RUN_TEST(test_error_callback_releases_cs_without_firing_callback);
    return UNITY_END();
}
