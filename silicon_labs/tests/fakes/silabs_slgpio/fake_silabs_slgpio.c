/* Fake current-driver (sl_gpio/sl_i2c/IADC) implementation - backs
 * sl_gpio.h/sl_i2c.h/em_iadc.h's declarations with host-runnable
 * bodies so this vendor's TARGET_SILABS_SLGPIO GPIO/I2C/ADC adapters
 * can be unit tested without real Silicon Labs silicon. Not a copy of
 * Silicon Labs' source.
 */
#include "fake_silabs_slgpio.h"
#include <string.h>

IADC_TypeDef fake_silabs_iadc0;

sl_status_t fake_sli2c_next_status = SL_STATUS_OK;
bool fake_sli2c_auto_complete = true;
sl_i2c_event_t fake_sli2c_next_event = SL_I2C_EVENT_COMPLETED;
uint32_t fake_iadc_next_value = 0;

static bool s_gpio_state[4][16];

void fake_silabs_slgpio_gpio_set_input(uint8_t port, uint8_t pin, bool state)
{
    if (port < 4 && pin < 16) {
        s_gpio_state[port][pin] = state;
    }
}

sl_status_t sl_gpio_init(void)
{
    return SL_STATUS_OK;
}

sl_status_t sl_gpio_set_pin_mode(sl_gpio_t *gpio, sl_gpio_mode_t mode, bool value)
{
    (void)gpio; (void)mode; (void)value;
    return SL_STATUS_OK;
}

void sl_gpio_set_pin(sl_gpio_t *gpio)
{
    if (gpio->port < 4 && gpio->pin < 16) { s_gpio_state[gpio->port][gpio->pin] = true; }
}

void sl_gpio_clear_pin(sl_gpio_t *gpio)
{
    if (gpio->port < 4 && gpio->pin < 16) { s_gpio_state[gpio->port][gpio->pin] = false; }
}

sl_status_t sl_gpio_get_pin_input(sl_gpio_t *gpio, bool *value)
{
    *value = (gpio->port < 4 && gpio->pin < 16) ? s_gpio_state[gpio->port][gpio->pin] : false;
    return SL_STATUS_OK;
}

void sl_gpio_toggle_pin(sl_gpio_t *gpio)
{
    if (gpio->port < 4 && gpio->pin < 16) {
        s_gpio_state[gpio->port][gpio->pin] = !s_gpio_state[gpio->port][gpio->pin];
    }
}

static struct sl_i2c_instance_s s_i2c_instances[2];
static int s_i2c_next_slot;
static sl_i2c_callback_t s_i2c_last_callback;
static void *s_i2c_last_context;

sl_status_t sl_i2c_init(sl_i2c_init_params_t const *params, sl_i2c_handle_t *handle)
{
    (void)params;
    if (s_i2c_next_slot < 2) {
        *handle = &s_i2c_instances[s_i2c_next_slot++];
    }
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_set_follower_address(sl_i2c_handle_t handle, uint8_t address)
{
    (void)handle; (void)address;
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_send_blocking(sl_i2c_handle_t handle, uint8_t const *data, uint16_t len, uint32_t timeout_ms)
{
    (void)handle; (void)data; (void)len; (void)timeout_ms;
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_receive_blocking(sl_i2c_handle_t handle, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)handle; (void)data; (void)len; (void)timeout_ms;
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_transfer(sl_i2c_handle_t handle, uint8_t const *tx_data, uint16_t tx_len,
                            uint8_t *rx_data, uint16_t rx_len)
{
    (void)handle; (void)tx_data; (void)tx_len; (void)rx_data; (void)rx_len;
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_send_non_blocking(sl_i2c_handle_t handle, uint8_t const *data, uint16_t len,
                                     sl_i2c_callback_t callback, void *context)
{
    (void)handle; (void)data; (void)len;
    s_i2c_last_callback = callback;
    s_i2c_last_context = context;
    if (fake_sli2c_auto_complete && callback != NULL) {
        callback(fake_sli2c_next_event, context);
    }
    return fake_sli2c_next_status;
}

sl_status_t sl_i2c_receive_non_blocking(sl_i2c_handle_t handle, uint8_t *data, uint16_t len,
                                        sl_i2c_callback_t callback, void *context)
{
    (void)handle; (void)data; (void)len;
    s_i2c_last_callback = callback;
    s_i2c_last_context = context;
    if (fake_sli2c_auto_complete && callback != NULL) {
        callback(fake_sli2c_next_event, context);
    }
    return fake_sli2c_next_status;
}

void fake_sli2c_fire_event(sl_i2c_event_t event)
{
    if (s_i2c_last_callback != NULL) {
        s_i2c_last_callback(event, s_i2c_last_context);
    }
}

void IADC_init(IADC_TypeDef *iadc, IADC_Init_t const *init, IADC_AllConfigs_t const *all_configs)
{
    (void)iadc; (void)init; (void)all_configs;
}

void IADC_initSingle(IADC_TypeDef *iadc, IADC_InitSingle_t const *init, IADC_SingleInput_t const *input)
{
    (void)iadc; (void)init; (void)input;
}

void IADC_command(IADC_TypeDef *iadc, IADC_Command_t cmd) { (void)iadc; (void)cmd; }
uint32_t IADC_getInt(IADC_TypeDef *iadc) { (void)iadc; return IADC_IF_SINGLEDONE; }
void IADC_clearInt(IADC_TypeDef *iadc, uint32_t flags) { (void)iadc; (void)flags; }
void IADC_enableInt(IADC_TypeDef *iadc, uint32_t flags) { (void)iadc; (void)flags; }
void IADC_disableInt(IADC_TypeDef *iadc, uint32_t flags) { (void)iadc; (void)flags; }

IADC_Result_t IADC_readSingleResult(IADC_TypeDef *iadc)
{
    IADC_Result_t result;
    (void)iadc;
    result.data = fake_iadc_next_value;
    return result;
}

/* ---- Timer (sl_sleeptimer) --------------------------------------------------- */

#define HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS 4

static sl_sleeptimer_timer_handle_t *s_sleeptimer_handle[HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS];
static sl_sleeptimer_timer_callback_t s_sleeptimer_callback[HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS];
static void *s_sleeptimer_data[HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS];
static int s_sleeptimer_next_slot;
static bool s_sleeptimer_initialized;

sl_status_t sl_sleeptimer_init(void)
{
    s_sleeptimer_initialized = true;
    return SL_STATUS_OK;
}

sl_status_t sl_sleeptimer_ms32_to_tick(uint32_t time_ms, uint32_t *tick)
{
    *tick = time_ms; /* fake fixed at 1 tick == 1 ms */
    return SL_STATUS_OK;
}

static int hf_test_sleeptimer_slot_of(sl_sleeptimer_timer_handle_t *handle)
{
    int i;
    for (i = 0; i < s_sleeptimer_next_slot; i++) {
        if (s_sleeptimer_handle[i] == handle) { return i; }
    }
    return -1;
}

static sl_status_t hf_test_sleeptimer_start(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout,
                                             sl_sleeptimer_timer_callback_t callback, void *callback_data,
                                             uint8_t priority, uint16_t option_flags)
{
    (void)timeout; (void)priority; (void)option_flags;
    int slot = hf_test_sleeptimer_slot_of(handle);
    if (slot < 0 && s_sleeptimer_next_slot < HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS) {
        slot = s_sleeptimer_next_slot++;
        s_sleeptimer_handle[slot] = handle;
    }
    if (slot >= 0) {
        s_sleeptimer_callback[slot] = callback;
        s_sleeptimer_data[slot] = callback_data;
    }
    return SL_STATUS_OK;
}

sl_status_t sl_sleeptimer_start_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout,
                                       sl_sleeptimer_timer_callback_t callback, void *callback_data,
                                       uint8_t priority, uint16_t option_flags)
{
    return hf_test_sleeptimer_start(handle, timeout, callback, callback_data, priority, option_flags);
}

sl_status_t sl_sleeptimer_start_periodic_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout,
                                                sl_sleeptimer_timer_callback_t callback, void *callback_data,
                                                uint8_t priority, uint16_t option_flags)
{
    return hf_test_sleeptimer_start(handle, timeout, callback, callback_data, priority, option_flags);
}

sl_status_t sl_sleeptimer_stop_timer(sl_sleeptimer_timer_handle_t *handle)
{
    (void)handle;
    return SL_STATUS_OK;
}

void fake_sl_sleeptimer_fire(int slot)
{
    if (slot >= 0 && slot < HAL_WRAPPER_FAKE_SLEEPTIMER_SLOTS && s_sleeptimer_callback[slot] != NULL) {
        s_sleeptimer_callback[slot](s_sleeptimer_handle[slot], s_sleeptimer_data[slot]);
    }
}

bool fake_sl_sleeptimer_is_initialized(void)
{
    return s_sleeptimer_initialized;
}

/* ---- PWM (sl_pwm) ------------------------------------------------------------- */

#define HAL_WRAPPER_FAKE_SL_PWM_SLOTS 8

static sl_pwm_instance_t *s_sl_pwm_ptr[HAL_WRAPPER_FAKE_SL_PWM_SLOTS];
static uint8_t s_sl_pwm_duty[HAL_WRAPPER_FAKE_SL_PWM_SLOTS];
static bool s_sl_pwm_running[HAL_WRAPPER_FAKE_SL_PWM_SLOTS];
static int s_sl_pwm_next_slot;

static int hf_test_sl_pwm_slot_of(sl_pwm_instance_t *pwm)
{
    int i;
    for (i = 0; i < s_sl_pwm_next_slot; i++) {
        if (s_sl_pwm_ptr[i] == pwm) { return i; }
    }
    return -1;
}

sl_status_t sl_pwm_init(sl_pwm_instance_t *pwm, sl_pwm_config_t *config)
{
    (void)config;
    int slot = hf_test_sl_pwm_slot_of(pwm);
    if (slot < 0 && s_sl_pwm_next_slot < HAL_WRAPPER_FAKE_SL_PWM_SLOTS) {
        slot = s_sl_pwm_next_slot++;
        s_sl_pwm_ptr[slot] = pwm;
    }
    return SL_STATUS_OK;
}

sl_status_t sl_pwm_deinit(sl_pwm_instance_t *pwm) { (void)pwm; return SL_STATUS_OK; }

void sl_pwm_start(sl_pwm_instance_t *pwm)
{
    int slot = hf_test_sl_pwm_slot_of(pwm);
    if (slot >= 0) { s_sl_pwm_running[slot] = true; }
}

void sl_pwm_stop(sl_pwm_instance_t *pwm)
{
    int slot = hf_test_sl_pwm_slot_of(pwm);
    if (slot >= 0) { s_sl_pwm_running[slot] = false; }
}

void sl_pwm_set_duty_cycle(sl_pwm_instance_t *pwm, uint8_t percent)
{
    int slot = hf_test_sl_pwm_slot_of(pwm);
    if (slot >= 0) { s_sl_pwm_duty[slot] = percent; }
}

uint8_t sl_pwm_get_duty_cycle(sl_pwm_instance_t *pwm)
{
    int slot = hf_test_sl_pwm_slot_of(pwm);
    return (slot >= 0) ? s_sl_pwm_duty[slot] : 0;
}

uint8_t fake_sl_pwm_get_duty_cycle(int slot)
{
    return (slot >= 0 && slot < HAL_WRAPPER_FAKE_SL_PWM_SLOTS) ? s_sl_pwm_duty[slot] : 0;
}

bool fake_sl_pwm_is_running(int slot)
{
    return (slot >= 0 && slot < HAL_WRAPPER_FAKE_SL_PWM_SLOTS) ? s_sl_pwm_running[slot] : false;
}

void fake_silabs_slgpio_reset(void)
{
    memset(s_gpio_state, 0, sizeof(s_gpio_state));
    memset(s_i2c_instances, 0, sizeof(s_i2c_instances));
    s_i2c_next_slot = 0;
    s_i2c_last_callback = NULL;
    s_i2c_last_context = NULL;
    fake_sli2c_next_status = SL_STATUS_OK;
    fake_sli2c_auto_complete = true;
    fake_sli2c_next_event = SL_I2C_EVENT_COMPLETED;
    fake_iadc_next_value = 0;
    memset(s_sleeptimer_handle, 0, sizeof(s_sleeptimer_handle));
    memset(s_sleeptimer_callback, 0, sizeof(s_sleeptimer_callback));
    memset(s_sleeptimer_data, 0, sizeof(s_sleeptimer_data));
    s_sleeptimer_next_slot = 0;
    s_sleeptimer_initialized = false;
    memset(s_sl_pwm_ptr, 0, sizeof(s_sl_pwm_ptr));
    memset(s_sl_pwm_duty, 0, sizeof(s_sl_pwm_duty));
    memset(s_sl_pwm_running, 0, sizeof(s_sl_pwm_running));
    s_sl_pwm_next_slot = 0;
}
