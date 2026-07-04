#include <stdio.h>
#include <math.h>
#include "bean_flight.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "bean_altimeter.h"
#include "bean_imu.h"
#include "bean_led.h"
#include "bean_bits.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "driver/gpio.h"

#define DUAL_DEPLOYMENT 0 // set to 1 to enable dual deployment, 0 for single deployment

#define FLIGHT_TASK_PRIORITY 20 // must outrank the WiFi/webui tasks, deployment timing depends on it
#define FLIGHT_TASK_CORE     1 // WiFi and lwip run on core 0, keep the flight loop away from them

#define REFERENCE_EMA_ALPHA 0.001 // ground reference smoothing while on the pad, ~10s time constant at 100Hz

const static char TAG[] = "BEAN_FLIGHT";

static bean_context_t *flight_ctx = NULL;
static TaskHandle_t flight_task_handle;
// One-shot timers end the pyro pulses so the state machine never blocks while a channel is firing
static esp_timer_handle_t drogue_off_timer;
static esp_timer_handle_t main_off_timer;

static uint8_t launch_counter            = 0; //counter for launch detection
static uint8_t apogee_counter            = 0; //counter for apogee detection
static int64_t takeoff_timestamp_ms      = 0; //timestamp of takeoff in ms
static int64_t apogee_timestamp_ms       = 0; //timestamp of apogee in ms
static int64_t last_baro_ok_ms           = 0; //timestamp of the last successful altimeter read, drives the failsafes
static float current_height              = 0;
static float current_recorded_max_height = 0;
static double reference_temperature      = 0; //ground reference temperature in K, tracked until launch
static double reference_pressure         = 0; //ground reference pressure in Pa, tracked until launch

static flight_state_t current_flight_state = FLIGHT_STATE_PRELAUNCH;

static const char *state_names[] = { "PRELAUNCH", "ARMED", "ASCENT", "DROGUE_OUT", "MAIN_OUT", "LANDED" };

const char *bean_flight_state_name(flight_state_t state)
{
    if ((unsigned)state >= sizeof(state_names) / sizeof(state_names[0]))
    {
        return "UNKNOWN";
    }
    return state_names[state];
}

flight_state_t bean_flight_get_state(void)
{
    return current_flight_state;
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool baro_is_stale(void)
{
    return (now_ms() - last_baro_ok_ms) > BARO_FAILSAFE_TIMEOUT;
}

static float calculate_acceleration_vector_magnitude(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

static double calculate_height(double pressure, double ref_temperature, double ref_pressure)
{ // calculate height from pressure, reference temperature and reference pressure at ground level
    return (ref_temperature / -0.0065) * (pow((pressure / ref_pressure), 0.190263) - 1.0);
}

static void pyro_off(void *arg)
{
    gpio_set_level((gpio_num_t)(intptr_t)arg, 0);
}

static void fire_pyro(gpio_num_t pin, esp_timer_handle_t off_timer)
{
    gpio_set_level(pin, 1);
    esp_timer_start_once(off_timer, (uint64_t)PYRO_ACTIVE_TIME * 1000);
}

// Track the ground reference until launch so height is measured against fresh pad
// conditions instead of a single power-on sample
static void update_ground_reference(void)
{
    double pressure    = bean_altimeter_get_pressure();
    double temperature = bean_altimeter_get_temperature() + 273.15;
    if (reference_pressure <= 0)
    { // no valid reference yet, take the first sample as-is
        reference_pressure    = pressure;
        reference_temperature = temperature;
        return;
    }
    reference_pressure += REFERENCE_EMA_ALPHA * (pressure - reference_pressure);
    reference_temperature += REFERENCE_EMA_ALPHA * (temperature - reference_temperature);
}

static void log_state_transition(flight_state_t state)
{
    if (flight_ctx == NULL || flight_ctx->data_log_queue == NULL)
    {
        return;
    }
    log_data_t log_data = { .measurement_type  = MEASUREMENT_TYPE_STAGE_TRANSITION,
                            .timestamp         = (uint32_t)esp_log_timestamp(),
                            .measurement_value = "" };
    snprintf(log_data.measurement_value, sizeof(log_data.measurement_value), "%s", bean_flight_state_name(state));
    xQueueSend(flight_ctx->data_log_queue, &log_data, 1); // Try 1 tick and then give up
}

esp_err_t bean_flight_goto_state(flight_state_t new_state)
{
    ESP_LOGI(TAG,
             "Trying to transition to state %s from state %s",
             bean_flight_state_name(new_state),
             bean_flight_state_name(current_flight_state));
    // handle any state transition logic here
    if (new_state == current_flight_state)
    {
        ESP_LOGW(TAG, "Already in state %s", bean_flight_state_name(new_state));
        return ESP_OK;
    }
    else if (new_state == FLIGHT_STATE_PRELAUNCH && current_flight_state != FLIGHT_STATE_ARMED)
    {
        ESP_LOGE(TAG, "Can only transition to PRELAUNCH state (disarm) from ARMED state");
        return ESP_FAIL;
    }
    else if (new_state == FLIGHT_STATE_ARMED && current_flight_state != FLIGHT_STATE_PRELAUNCH)
    {
        ESP_LOGE(TAG, "Can only transition to ARMED state from PRELAUNCH state");
        return ESP_FAIL;
    }
    else if (new_state == FLIGHT_STATE_ASCENT && current_flight_state != FLIGHT_STATE_ARMED)
    {
        ESP_LOGE(TAG, "Can only transition to ASCENT state from ARMED state");
        return ESP_FAIL;
    }
    else if (new_state == FLIGHT_STATE_DROGUE_OUT && current_flight_state != FLIGHT_STATE_ASCENT)
    {
        ESP_LOGE(TAG, "Can only transition to DROGUE_OUT state from ASCENT state");
        return ESP_FAIL;
    }
    else if (new_state == FLIGHT_STATE_MAIN_OUT &&
             ((current_flight_state != FLIGHT_STATE_DROGUE_OUT) && (current_flight_state != FLIGHT_STATE_ASCENT)))
    {
        ESP_LOGE(TAG, "Can only transition to MAIN_OUT state from DROGUE_OUT state");
        return ESP_FAIL;
    }
    else if (new_state == FLIGHT_STATE_LANDED && current_flight_state != FLIGHT_STATE_MAIN_OUT)
    {
        ESP_LOGE(TAG, "Can only transition to LANDED state from MAIN_OUT state");
        return ESP_FAIL;
    }

    if (new_state == FLIGHT_STATE_PRELAUNCH)
    {
        launch_counter = 0;
        apogee_counter = 0;
        ESP_LOGI(TAG, "Rocket disarmed, waiting to be armed");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 0, .g = 0, .b = 0 }); // Off
    }
    else if (new_state == FLIGHT_STATE_ARMED)
    {
        launch_counter = 0;
        ESP_LOGI(TAG, "Rocket armed, waiting for launch");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 255, .g = 255, .b = 0 }); // Yellow
    }
    else if (new_state == FLIGHT_STATE_ASCENT)
    {
        current_height              = 0; //VERY IMPORTANT to reset height and max height on launch
        current_recorded_max_height = 0; //VERY IMPORTANT to reset height and max height on launch
        ESP_LOGI(TAG, "Launch detected, rocket ascending");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 255, .g = 165, .b = 0 }); // Orange
    }
    else if (new_state == FLIGHT_STATE_DROGUE_OUT)
    {
        fire_pyro(DROGUE_CHUTE, drogue_off_timer);
        ESP_LOGI(TAG, "Deploying drogue chute");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 0, .g = 0, .b = 255 }); // Blue
    }
    else if (new_state == FLIGHT_STATE_MAIN_OUT)
    {
        fire_pyro(MAIN_CHUTE, main_off_timer);
        ESP_LOGI(TAG, "Deploying main chute");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 0, .g = 255, .b = 0 }); // Green
    }
    else if (new_state == FLIGHT_STATE_LANDED)
    {
        //TODO: handle landed state, stop logging?
        ESP_LOGI(TAG, "Rocket has landed");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 255, .g = 0, .b = 255 }); // Magenta
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    current_flight_state = new_state;
    log_state_transition(new_state);
    ESP_LOGI(TAG, "Transitioned to state %s", bean_flight_state_name(new_state));
    return ESP_OK;
}

static esp_err_t bean_flight_process_landed(bean_context_t *ctx)
{
    // landed state, do nothing or log data
    return ESP_OK;
}

static esp_err_t bean_flight_process_main_out(bean_context_t *ctx)
{
    //check for landing conditions, TODO: based on timer and mean altitude over 10 seconds staying withing a range (peak-to-peak)
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height < LANDED_HEIGHT_THRESHOLD ||
        (now_ms() > (apogee_timestamp_ms +
                     LANDED_TIME_AFTER_MAIN))) //check if height is lower than threshold or time after drogue deployment
    {
        bean_flight_goto_state(FLIGHT_STATE_LANDED);
    }
    return ESP_OK;
}

static esp_err_t bean_flight_process_drogue_out(bean_context_t *ctx)
{
    //check for altitude treshold to deploy main, based on altimeter reading
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height < MAIN_HEIGHT_DEPLOY) //check if height is lower than threshold and time after drogue deployment
    {
        bean_flight_goto_state(FLIGHT_STATE_MAIN_OUT);
        return ESP_OK;
    }
    // failsafe: the altimeter stopped delivering data, deploy main on the timer instead of never
    if (baro_is_stale() && (now_ms() > (apogee_timestamp_ms + MAIN_TIME_AFTER_DROGUE)))
    {
        ESP_LOGE(TAG, "Altimeter lost after drogue deployment, failsafe main deployment");
        bean_flight_goto_state(FLIGHT_STATE_MAIN_OUT);
    }
    return ESP_OK;
}

static esp_err_t bean_flight_process_ascent(bean_context_t *ctx)
{
    // check for apogee conditions, alt based & timer as safety
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height > current_recorded_max_height) //update max height during flight
    {
        current_recorded_max_height = current_height;
    }
    if ((current_height < (current_recorded_max_height - APOGEE_HEIGHT_THRESHOLD)) &&
        (now_ms() >
         (takeoff_timestamp_ms + APOGEE_MIN_TIME_AFTER_LAUNCH))) //check if height is lower than max height - threshold
    {
        apogee_counter++;
        if (
          apogee_counter >
          APOGEE_HEIGHT_COUNT) //check if height is lower than max height - threshold for 10 consecutive readings -> Apogee detected
        {
            apogee_timestamp_ms = now_ms(); //take a timestamp for the apogee time, needed for main deployment timing
#if DUAL_DEPLOYMENT
            bean_flight_goto_state(FLIGHT_STATE_DROGUE_OUT);
#else
            bean_flight_goto_state(FLIGHT_STATE_MAIN_OUT);
#endif
            return ESP_OK;
        }
    }
    else
    {
        apogee_counter = 0;
    }
    // failsafe: the altimeter stopped delivering data mid-flight, apogee detection is blind so
    // deploy once the minimum flight time has passed instead of never
    if (baro_is_stale() && (now_ms() > (takeoff_timestamp_ms + APOGEE_MIN_TIME_AFTER_LAUNCH)))
    {
        ESP_LOGE(TAG, "Altimeter lost during ascent, failsafe deployment");
        apogee_timestamp_ms = now_ms();
#if DUAL_DEPLOYMENT
        bean_flight_goto_state(FLIGHT_STATE_DROGUE_OUT);
#else
        bean_flight_goto_state(FLIGHT_STATE_MAIN_OUT);
#endif
        return ESP_OK;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);

    return ESP_OK;
}

static esp_err_t bean_flight_process_armed(bean_context_t *ctx)
{
    if (!(xEventGroupGetBits(ctx->system_event_group) & BEAN_SYSTEM_ARMED))
    { // disarmed via the web UI, go back to waiting
        return bean_flight_goto_state(FLIGHT_STATE_PRELAUNCH);
    }

    update_ground_reference();

    //check for takeoff conditions,accel based
    float accel_mag =
      calculate_acceleration_vector_magnitude(get_x_accel_data(), get_y_accel_data(), get_z_accel_data());

    if (accel_mag > LAUNCH_ACCEL_THRESHOLD)
    {
        ESP_LOGI(TAG, "Launch acceleration detected: %f g", accel_mag);
        launch_counter++;
        if (launch_counter > LAUNCH_ACCEL_COUNT)
        {
            ESP_LOGI(TAG, "YEET!");
            takeoff_timestamp_ms = now_ms(); //take a timestamp for the takeoff time, needed for apogee detection
            bean_flight_goto_state(FLIGHT_STATE_ASCENT);
            return ESP_OK;
        }
    }
    else
    {
        launch_counter = 0;
    }

    return ESP_OK;
}

static esp_err_t bean_flight_process_prelaunch(bean_context_t *ctx)
{
    update_ground_reference();
    // arming is an explicit decision made via the web UI, never a timer
    EventBits_t bits =
      xEventGroupWaitBits(ctx->system_event_group, BEAN_SYSTEM_ARMED, pdFALSE, pdTRUE, pdMS_TO_TICKS(1000));
    if (bits & BEAN_SYSTEM_ARMED)
    {
        bean_flight_goto_state(FLIGHT_STATE_ARMED);
    }
    return ESP_OK;
}

esp_err_t log_measurements(bean_context_t *ctx)
{
    uint32_t timestamp  = (uint32_t)esp_log_timestamp();
    log_data_t log_data = { .measurement_type  = MEASUREMENT_TYPE_ACCELERATION_X,
                            .timestamp         = timestamp,
                            .measurement_value = "" };
    snprintf(log_data.measurement_value, sizeof(log_data.measurement_value), "%f", get_x_accel_data());
    xQueueSend(ctx->data_log_queue, &log_data, 1); // Try 1 tick and then give up

    log_data.measurement_type = MEASUREMENT_TYPE_ACCELERATION_Y;
    snprintf(log_data.measurement_value, sizeof(log_data.measurement_value), "%f", get_y_accel_data());
    xQueueSend(ctx->data_log_queue, &log_data, 1); // Try 1 tick and then give up

    log_data.measurement_type = MEASUREMENT_TYPE_ACCELERATION_Z;
    snprintf(log_data.measurement_value, sizeof(log_data.measurement_value), "%f", get_z_accel_data());
    xQueueSend(ctx->data_log_queue, &log_data, 1); // Try 1 tick and then give up

    return ESP_OK;
}

static void flight_task(void *arg)
{
    bean_context_t *ctx = (bean_context_t *)arg;
    while (1)
    {
        // The flight task owns its sensor reads: deployment decisions must never
        // depend on another task (e.g. the web UI sampler) polling for us
        bean_imu_update_accel();
        if (bean_altimeter_update() == ESP_OK)
        {
            last_baro_ok_ms = now_ms();
        }

        switch (current_flight_state)
        {
        case FLIGHT_STATE_PRELAUNCH:
            bean_flight_process_prelaunch(ctx);
            break;
        case FLIGHT_STATE_ARMED:
            bean_flight_process_armed(ctx);
            break;
        case FLIGHT_STATE_ASCENT:
            bean_flight_process_ascent(ctx);
            break;
        case FLIGHT_STATE_DROGUE_OUT:
            bean_flight_process_drogue_out(ctx);
            break;
        case FLIGHT_STATE_MAIN_OUT:
            bean_flight_process_main_out(ctx);
            break;
        case FLIGHT_STATE_LANDED:
            bean_flight_process_landed(ctx);
            break;
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // run every 10ms // about 1 tick
    }
}

esp_err_t bean_flight_init(bean_context_t *ctx)
{
    ESP_LOGI(TAG, "Initializing Bean Flight");
    flight_ctx = ctx;

    // The pyro outputs must be configured and actively driven low before anything else happens
    gpio_config_t pyro_config = {
        .pin_bit_mask = (1ULL << DROGUE_CHUTE) | (1ULL << MAIN_CHUTE),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pyro_config), TAG, "Pyro GPIO config failed");
    gpio_set_level(DROGUE_CHUTE, 0);
    gpio_set_level(MAIN_CHUTE, 0);

    const esp_timer_create_args_t drogue_off_args = { .callback = &pyro_off,
                                                      .arg      = (void *)(intptr_t)DROGUE_CHUTE,
                                                      .name     = "drogue_pyro_off" };
    const esp_timer_create_args_t main_off_args   = { .callback = &pyro_off,
                                                      .arg      = (void *)(intptr_t)MAIN_CHUTE,
                                                      .name     = "main_pyro_off" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&drogue_off_args, &drogue_off_timer), TAG, "Drogue pyro timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_create(&main_off_args, &main_off_timer), TAG, "Main pyro timer create failed");

    if (bean_altimeter_update() == ESP_OK)
    {
        last_baro_ok_ms = now_ms();
        update_ground_reference();
        ESP_LOGI(TAG, "Reference pressure: %f Pa", reference_pressure);
        ESP_LOGI(TAG, "Reference temperature: %f K", reference_temperature);
    }
    else
    { // don't fail the whole boot over this, the reference converges once reads succeed
        ESP_LOGW(TAG, "Initial altimeter read failed, ground reference not yet valid");
    }

    xTaskCreatePinnedToCore(
      &flight_task, "flight_task", 1024 * 4, (void *)ctx, FLIGHT_TASK_PRIORITY, &flight_task_handle, FLIGHT_TASK_CORE);

    if (flight_task_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create flight task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
