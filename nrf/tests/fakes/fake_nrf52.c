/* Fake nrfx implementation - backs nrf_gpio.h/nrfx_uarte.h/nrfx_twim.h/
 * nrfx_spim.h/nrfx_saadc.h's declarations with host-runnable bodies so
 * this vendor's adapters under nrf/ can be unit tested without real
 * Nordic silicon. Not a copy of Nordic's nrfx source.
 *
 * Every real transfer on this vendor goes through a registered event
 * handler rather than a weak-callback-override symbol the way STM32's
 * fake can invoke directly - so by default (fake_nrf52_auto_complete
 * == true), a successful kick-off (nrfx_uarte_tx/nrfx_twim_xfer/
 * nrfx_spim_xfer) immediately invokes the handler that was registered
 * for that instance with a DONE event, synchronously, before
 * returning. This is what lets this vendor's "kick off, then spin on
 * our own busy flag" blocking adapter functions terminate at all on a
 * host machine with no real interrupt. Tests that need to control
 * exactly when completion happens (checking that a callback hasn't
 * fired yet, or firing an ERROR event instead) set
 * fake_nrf52_auto_complete = false first, call the async function,
 * then fire the event manually via one of the fake_nrf52_*_fire_event
 * helpers below.
 */
#include "fake_nrf52.h"
#include <string.h>

nrfx_err_t fake_nrf52_next_status = NRFX_SUCCESS;
nrf_saadc_value_t fake_nrf52_saadc_next_blocking_value = 0;
bool fake_nrf52_auto_complete = true;
bool fake_nrf52_twim_auto_success = true;
uint32_t fake_nrf52_last_erase_address;
int fake_nrf52_erase_call_count;
uint32_t fake_nrf52_last_write_address;
uint32_t fake_nrf52_last_write_num_words;
int fake_nrf52_write_call_count;

static bool s_gpio_state[64];

/* ---- GPIO -------------------------------------------------------------- */

void fake_nrf52_gpio_set_input(uint32_t pin, bool state)
{
    if (pin < 64) {
        s_gpio_state[pin] = state;
    }
}

void nrf_gpio_cfg_input(uint32_t pin, nrf_gpio_pin_pull_t pull) { (void)pin; (void)pull; }
void nrf_gpio_cfg_output(uint32_t pin) { (void)pin; }
void nrf_gpio_cfg(uint32_t pin, nrf_gpio_pin_dir_t dir, nrf_gpio_pin_input_t input,
                  nrf_gpio_pin_pull_t pull, nrf_gpio_pin_drive_t drive, nrf_gpio_pin_sense_t sense)
{
    (void)pin; (void)dir; (void)input; (void)pull; (void)drive; (void)sense;
}

void nrf_gpio_pin_set(uint32_t pin) { if (pin < 64) { s_gpio_state[pin] = true; } }
void nrf_gpio_pin_clear(uint32_t pin) { if (pin < 64) { s_gpio_state[pin] = false; } }
uint32_t nrf_gpio_pin_read(uint32_t pin) { return (pin < 64 && s_gpio_state[pin]) ? 1U : 0U; }
void nrf_gpio_pin_toggle(uint32_t pin) { if (pin < 64) { s_gpio_state[pin] = !s_gpio_state[pin]; } }

/* ---- UART (nrfx_uarte) --------------------------------------------------- */

static nrfx_uarte_event_handler_t s_uarte_handler[2];
static nrfx_uarte_t const *s_uarte_ptr[2];
static int s_uarte_next_slot;

static int hf_test_uarte_slot_of(nrfx_uarte_t const *p_instance)
{
    int i;
    for (i = 0; i < s_uarte_next_slot; i++) {
        if (s_uarte_ptr[i] == p_instance) { return i; }
    }
    return -1;
}

nrfx_err_t nrfx_uarte_init(nrfx_uarte_t const *p_instance, nrfx_uarte_config_t const *p_config,
                            nrfx_uarte_event_handler_t handler)
{
    (void)p_config;
    if (s_uarte_next_slot < 2) {
        s_uarte_ptr[s_uarte_next_slot] = p_instance;
        s_uarte_handler[s_uarte_next_slot] = handler;
        s_uarte_next_slot++;
    }
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_uarte_rx_buffer_set(nrfx_uarte_t const *p_instance, uint8_t *p_data, size_t length)
{
    (void)p_instance; (void)p_data; (void)length;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_uarte_rx_enable(nrfx_uarte_t const *p_instance, uint32_t flags)
{
    (void)p_instance; (void)flags;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_uarte_tx(nrfx_uarte_t const *p_instance, uint8_t const *p_data, size_t length, uint32_t flags)
{
    (void)p_data; (void)length; (void)flags;
    if (fake_nrf52_auto_complete && fake_nrf52_next_status == NRFX_SUCCESS) {
        int slot = hf_test_uarte_slot_of(p_instance);
        if (slot >= 0 && s_uarte_handler[slot] != NULL) {
            nrfx_uarte_event_t event;
            event.type = NRFX_UARTE_EVT_TX_DONE;
            s_uarte_handler[slot](&event, NULL);
        }
    }
    return fake_nrf52_next_status;
}

void fake_nrf52_uarte_fire_event(int slot, nrfx_uarte_event_t const *event)
{
    if (slot >= 0 && slot < 2 && s_uarte_handler[slot] != NULL) {
        s_uarte_handler[slot](event, NULL);
    }
}

/* ---- I2C (nrfx_twim) ---------------------------------------------------- */

static nrfx_twim_event_handler_t s_twim_handler[2];
static void *s_twim_context[2];
static nrfx_twim_t const *s_twim_ptr[2];
static int s_twim_next_slot;

static int hf_test_twim_slot_of(nrfx_twim_t const *p_instance)
{
    int i;
    for (i = 0; i < s_twim_next_slot; i++) {
        if (s_twim_ptr[i] == p_instance) { return i; }
    }
    return -1;
}

nrfx_err_t nrfx_twim_init(nrfx_twim_t const *p_instance, nrfx_twim_config_t const *p_config,
                          nrfx_twim_event_handler_t handler, void *p_context)
{
    (void)p_config;
    if (s_twim_next_slot < 2) {
        s_twim_ptr[s_twim_next_slot] = p_instance;
        s_twim_handler[s_twim_next_slot] = handler;
        s_twim_context[s_twim_next_slot] = p_context;
        s_twim_next_slot++;
    }
    return fake_nrf52_next_status;
}

void nrfx_twim_enable(nrfx_twim_t const *p_instance) { (void)p_instance; }

nrfx_err_t nrfx_twim_xfer(nrfx_twim_t const *p_instance, nrfx_twim_xfer_desc_t const *p_xfer_desc, uint32_t flags)
{
    (void)flags;
    if (fake_nrf52_auto_complete && fake_nrf52_next_status == NRFX_SUCCESS) {
        int slot = hf_test_twim_slot_of(p_instance);
        if (slot >= 0 && s_twim_handler[slot] != NULL) {
            nrfx_twim_event_t event;
            event.type = fake_nrf52_twim_auto_success ? NRFX_TWIM_EVT_DONE : NRFX_TWIM_EVT_ADDRESS_NACK;
            event.xfer_desc = *p_xfer_desc;
            s_twim_handler[slot](&event, s_twim_context[slot]);
        }
    }
    return fake_nrf52_next_status;
}

void fake_nrf52_twim_fire_event(int slot, nrfx_twim_event_t const *event)
{
    if (slot >= 0 && slot < 2 && s_twim_handler[slot] != NULL) {
        s_twim_handler[slot](event, s_twim_context[slot]);
    }
}

/* ---- SPI (nrfx_spim) ----------------------------------------------------- */

static nrfx_spim_event_handler_t s_spim_handler[2];
static void *s_spim_context[2];
static nrfx_spim_t const *s_spim_ptr[2];
static int s_spim_next_slot;

static int hf_test_spim_slot_of(nrfx_spim_t const *p_instance)
{
    int i;
    for (i = 0; i < s_spim_next_slot; i++) {
        if (s_spim_ptr[i] == p_instance) { return i; }
    }
    return -1;
}

nrfx_err_t nrfx_spim_init(nrfx_spim_t const *p_instance, nrfx_spim_config_t const *p_config,
                          nrfx_spim_event_handler_t handler, void *p_context)
{
    (void)p_config;
    if (s_spim_next_slot < 2) {
        s_spim_ptr[s_spim_next_slot] = p_instance;
        s_spim_handler[s_spim_next_slot] = handler;
        s_spim_context[s_spim_next_slot] = p_context;
        s_spim_next_slot++;
    }
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_spim_xfer(nrfx_spim_t const *p_instance, nrfx_spim_xfer_desc_t const *p_xfer_desc, uint32_t flags)
{
    (void)p_xfer_desc; (void)flags;
    if (fake_nrf52_auto_complete && fake_nrf52_next_status == NRFX_SUCCESS) {
        int slot = hf_test_spim_slot_of(p_instance);
        if (slot >= 0 && s_spim_handler[slot] != NULL) {
            nrfx_spim_evt_t event;
            event.type = NRFX_SPIM_EVENT_DONE;
            s_spim_handler[slot](&event, s_spim_context[slot]);
        }
    }
    return fake_nrf52_next_status;
}

void fake_nrf52_spim_fire_event(int slot, nrfx_spim_evt_t const *event)
{
    if (slot >= 0 && slot < 2 && s_spim_handler[slot] != NULL) {
        s_spim_handler[slot](event, s_spim_context[slot]);
    }
}

/* ---- ADC (nrfx_saadc) ------------------------------------------------------ */

static nrfx_saadc_event_handler_t s_saadc_handler;
static nrf_saadc_value_t *s_saadc_buffer;
static uint16_t s_saadc_buffer_size;

nrfx_err_t nrfx_saadc_init(uint8_t irq_priority)
{
    (void)irq_priority;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_saadc_channel_config(nrfx_saadc_channel_t const *p_config)
{
    (void)p_config;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_saadc_simple_mode_set(uint32_t channel_mask, nrf_saadc_resolution_t resolution,
                                      nrf_saadc_oversample_t oversampling, nrfx_saadc_event_handler_t handler)
{
    (void)channel_mask; (void)resolution; (void)oversampling;
    s_saadc_handler = handler;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_saadc_buffer_set(nrf_saadc_value_t *p_buffer, uint16_t size)
{
    s_saadc_buffer = p_buffer;
    s_saadc_buffer_size = size;
    return fake_nrf52_next_status;
}

nrfx_err_t nrfx_saadc_mode_trigger(void)
{
    if (s_saadc_handler == NULL && s_saadc_buffer != NULL && s_saadc_buffer_size > 0) {
        /* Blocking mode (NULL handler, per nrf/adc/hal_port_nrf52_adc.c's
         * documented finding) - simulate the conversion completing
         * synchronously, like the real driver does. */
        s_saadc_buffer[0] = fake_nrf52_saadc_next_blocking_value;
    }
    return fake_nrf52_next_status;
}

void fake_nrf52_saadc_fire_done(nrf_saadc_value_t sample)
{
    if (s_saadc_handler != NULL) {
        nrf_saadc_value_t buffer[1];
        buffer[0] = sample;
        nrfx_saadc_evt_t event;
        event.type = NRFX_SAADC_EVT_DONE;
        event.data.done.p_buffer = buffer;
        event.data.done.size = 1;
        s_saadc_handler(&event);
    }
}

/* ---- Timer (nrfx_timer) --------------------------------------------------- */

static nrfx_timer_t *s_timer_ptr[2];
static nrfx_timer_event_handler_t s_timer_handler[2];
static void *s_timer_context[2];
static int s_timer_next_slot;

static int hf_test_timer_slot_of(nrfx_timer_t *p_instance)
{
    int i;
    for (i = 0; i < s_timer_next_slot; i++) {
        if (s_timer_ptr[i] == p_instance) { return i; }
    }
    return -1;
}

nrfx_err_t nrfx_timer_init(nrfx_timer_t *p_instance, nrfx_timer_config_t const *p_config,
                            nrfx_timer_event_handler_t handler)
{
    int slot = hf_test_timer_slot_of(p_instance);
    if (slot < 0 && s_timer_next_slot < 2) {
        slot = s_timer_next_slot++;
        s_timer_ptr[slot] = p_instance;
    }
    if (slot >= 0) {
        s_timer_handler[slot] = handler;
        s_timer_context[slot] = p_config->p_context;
    }
    return fake_nrf52_next_status;
}

uint32_t nrfx_timer_us_to_ticks(nrfx_timer_t *p_instance, uint32_t timer_us)
{
    (void)p_instance;
    return timer_us; /* fake fixed at 1 tick == 1 us, matching the adapter's own 1 MHz config */
}

void nrfx_timer_extended_compare(nrfx_timer_t *p_instance, uint32_t channel, uint32_t value,
                                  nrf_timer_short_mask_t short_mask, bool enable_int)
{
    (void)p_instance; (void)channel; (void)value; (void)short_mask; (void)enable_int;
}

void nrfx_timer_clear(nrfx_timer_t *p_instance) { (void)p_instance; }
void nrfx_timer_enable(nrfx_timer_t *p_instance) { (void)p_instance; }
void nrfx_timer_disable(nrfx_timer_t *p_instance) { (void)p_instance; }

void fake_nrf52_timer_fire_event(int slot, nrf_timer_event_t event_type)
{
    if (slot >= 0 && slot < 2 && s_timer_handler[slot] != NULL) {
        s_timer_handler[slot](event_type, s_timer_context[slot]);
    }
}

/* ---- PWM (nrfx_pwm) -------------------------------------------------------- */

static nrfx_pwm_t *s_pwm_ptr[2];
static nrf_pwm_sequence_t const *s_pwm_last_seq[2];
static bool s_pwm_playing[2];
static int s_pwm_next_slot;

static int hf_test_pwm_slot_of(nrfx_pwm_t *p_instance)
{
    int i;
    for (i = 0; i < s_pwm_next_slot; i++) {
        if (s_pwm_ptr[i] == p_instance) { return i; }
    }
    return -1;
}

nrfx_err_t nrfx_pwm_init(nrfx_pwm_t *p_instance, nrfx_pwm_config_t const *p_config,
                          nrfx_pwm_handler_t handler, void *p_context)
{
    (void)p_config; (void)handler; (void)p_context;
    if (hf_test_pwm_slot_of(p_instance) < 0 && s_pwm_next_slot < 2) {
        s_pwm_ptr[s_pwm_next_slot] = p_instance;
        s_pwm_next_slot++;
    }
    return fake_nrf52_next_status;
}

uint32_t nrfx_pwm_simple_playback(nrfx_pwm_t *p_instance, nrf_pwm_sequence_t const *p_sequence,
                                   uint16_t playback_count, uint32_t flags)
{
    (void)playback_count; (void)flags;
    int slot = hf_test_pwm_slot_of(p_instance);
    if (slot >= 0) {
        s_pwm_last_seq[slot] = p_sequence;
        s_pwm_playing[slot] = true;
    }
    return 0;
}

void nrfx_pwm_sequence_update(nrfx_pwm_t *p_instance, uint8_t seq_id, nrf_pwm_sequence_t const *p_sequence)
{
    (void)seq_id;
    int slot = hf_test_pwm_slot_of(p_instance);
    if (slot >= 0) {
        s_pwm_last_seq[slot] = p_sequence;
    }
}

bool nrfx_pwm_stop(nrfx_pwm_t *p_instance, bool wait_until_stopped)
{
    (void)wait_until_stopped;
    int slot = hf_test_pwm_slot_of(p_instance);
    if (slot >= 0) {
        s_pwm_playing[slot] = false;
    }
    return true;
}

bool fake_nrf52_pwm_is_playing(int slot)
{
    return (slot >= 0 && slot < 2) ? s_pwm_playing[slot] : false;
}

nrf_pwm_sequence_t const *fake_nrf52_pwm_last_sequence(int slot)
{
    return (slot >= 0 && slot < 2) ? s_pwm_last_seq[slot] : NULL;
}

void fake_nrf52_reset(void)
{
    fake_nrf52_next_status = NRFX_SUCCESS;
    fake_nrf52_saadc_next_blocking_value = 0;
    fake_nrf52_auto_complete = true;
    fake_nrf52_twim_auto_success = true;
    memset(s_gpio_state, 0, sizeof(s_gpio_state));
    memset(s_uarte_handler, 0, sizeof(s_uarte_handler));
    memset(s_uarte_ptr, 0, sizeof(s_uarte_ptr));
    s_uarte_next_slot = 0;
    memset(s_twim_handler, 0, sizeof(s_twim_handler));
    memset(s_twim_context, 0, sizeof(s_twim_context));
    memset(s_twim_ptr, 0, sizeof(s_twim_ptr));
    s_twim_next_slot = 0;
    memset(s_spim_handler, 0, sizeof(s_spim_handler));
    memset(s_spim_context, 0, sizeof(s_spim_context));
    memset(s_spim_ptr, 0, sizeof(s_spim_ptr));
    s_spim_next_slot = 0;
    s_saadc_handler = NULL;
    s_saadc_buffer = NULL;
    s_saadc_buffer_size = 0;
    memset(s_timer_ptr, 0, sizeof(s_timer_ptr));
    memset(s_timer_handler, 0, sizeof(s_timer_handler));
    memset(s_timer_context, 0, sizeof(s_timer_context));
    s_timer_next_slot = 0;
    memset(s_pwm_ptr, 0, sizeof(s_pwm_ptr));
    memset(s_pwm_last_seq, 0, sizeof(s_pwm_last_seq));
    memset(s_pwm_playing, 0, sizeof(s_pwm_playing));
    s_pwm_next_slot = 0;
    fake_nrf52_last_erase_address = 0;
    fake_nrf52_erase_call_count = 0;
    fake_nrf52_last_write_address = 0;
    fake_nrf52_last_write_num_words = 0;
    fake_nrf52_write_call_count = 0;
}

/* ---- Flash (nrfx_nvmc) --------------------------------------------------- */

nrfx_err_t nrfx_nvmc_page_erase(uint32_t address)
{
    fake_nrf52_last_erase_address = address;
    fake_nrf52_erase_call_count++;
    return fake_nrf52_next_status;
}

void nrfx_nvmc_words_write(uint32_t address, void const *src, uint32_t num_words)
{
    (void)src;
    fake_nrf52_last_write_address = address;
    fake_nrf52_last_write_num_words = num_words;
    fake_nrf52_write_call_count++;
}

bool nrfx_nvmc_write_done_check(void)
{
    return true;
}
