#include <stdio.h>
#include "bean_core.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "bean_altimeter.h"
#include "bean_imu.h"
#include "bean_led.h"
#include "esp_timer.h"

#define DUAL_DEPLOYMENT 0 // set to 1 to enable dual deployment, 0 for single deployment

const static char TAG[] = "BEAN_CORE";

static TaskHandle_t core_task_handle; // handle of the core task
uint8_t launch_counter            = 0; //counter for launch detection
uint8_t apogee_counter            = 0; //counter for apogee detection
int64_t takeoff_timestamp_ms      = 0; //timestamp of takeoff in ms
int64_t apogee_timestamp_ms       = 0; //timestamp of apogee in ms
float current_height              = 0;
float current_recorded_max_height = 0;
double reference_temperature      = 0; //reference temperature at launch
double reference_pressure         = 0; //reference pressure at launch

static core_flight_state_t current_flight_state = FLIGHT_STATE_PRELAUNCH;

esp_err_t log_measurements(bean_context_t *ctx);

esp_err_t bean_core_goto_state(core_flight_state_t new_state)
{
    ESP_LOGI(TAG, "Trying to transition to state %d from state %d", new_state, current_flight_state);
    // handle any state transition logic here
    if (new_state == current_flight_state)
    {
        ESP_LOGW(TAG, "Already in state %d", new_state);
        return ESP_OK;
    }
    else if (new_state == FLIGHT_STATE_PRELAUNCH)
    {
        ESP_LOGE(TAG, "Cannot transition back to PRELAUNCH state");
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

    if (new_state == FLIGHT_STATE_ARMED)
    {
        ESP_LOGI(TAG, "Rocket armed, waiting for launch");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 255, .g = 255, .b = 0 }); // Red
    }
    else if (new_state == FLIGHT_STATE_ASCENT)
    {
        current_height              = 0; //VERY IMPORTANT to reset height and max height on launch
        current_recorded_max_height = 0; //VERY IMPORTANT to reset height and max height on launch

        reference_temperature =
          bean_altimeter_get_temperature() +
          273.15; //recalibrate reference temperature and pressure, not very acurate because rocket is aleady moving. Moving average before launch better
        reference_pressure = bean_altimeter_get_pressure();
        ESP_LOGI(TAG, "Launch detected, rocket ascending");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 255, .g = 165, .b = 0 }); // Orange
    }
    else if (new_state == FLIGHT_STATE_DROGUE_OUT)
    {
        gpio_set_level(DROGUE_CHUTE, 1);
        vTaskDelay(PYRO_ACTIVE_TIME / portTICK_PERIOD_MS);
        gpio_set_level(DROGUE_CHUTE, 0);
        ESP_LOGI(TAG, "Deploying drogue chute");
        bean_led_set_color(LED_L1, (led_color_rgb_t){ .r = 0, .g = 0, .b = 255 }); // Blue
    }
    else if (new_state == FLIGHT_STATE_MAIN_OUT)
    {
        gpio_set_level(MAIN_CHUTE, 1);
        vTaskDelay(PYRO_ACTIVE_TIME / portTICK_PERIOD_MS);
        gpio_set_level(MAIN_CHUTE, 0);
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
    ESP_LOGI(TAG, "Transitioned to state %d", new_state);
    return ESP_OK;
}

esp_err_t bean_core_process_landed(bean_context_t *ctx)
{
    // landed state, do nothing or log data
    return ESP_OK;
}

esp_err_t bean_core_process_main_out(bean_context_t *ctx)
{
    //check for landing conditions, TODO: based on timer and mean altitude over 10 seconds staying withing a range (peak-to-peak)
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height < LANDED_HEIGHT_THRESHOLD ||
        (esp_timer_get_time() / 1000 >
         (apogee_timestamp_ms +
          LANDED_TIME_AFTER_MAIN))) //check if height is lower than threshold or time after drogue deployment
    {
        bean_core_goto_state(FLIGHT_STATE_LANDED);
    }
    return ESP_OK;
}

esp_err_t bean_core_process_drogue_out(bean_context_t *ctx)
{
    //check for altitude treshold to deploy main, based on altimeter reading
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height < MAIN_HEIGHT_DEPLOY) //check if height is lower than threshold and time after drogue deployment
    {
        bean_core_goto_state(FLIGHT_STATE_MAIN_OUT);
        return ESP_OK;
    }
    return ESP_OK;
}

esp_err_t bean_core_process_ascent(bean_context_t *ctx)
{
    // check for apogee conditions, alt based & timer as safety
    bean_altimeter_update();
    log_measurements(ctx);
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    if (current_height > current_recorded_max_height) //update max height during flight
    {
        current_recorded_max_height = current_height;
    }
    if ((current_height < (current_recorded_max_height - APOGEE_HEIGHT_THRESHOLD)) &&
        (esp_timer_get_time() / 1000 >
         (takeoff_timestamp_ms + APOGEE_MIN_TIME_AFTER_LAUNCH))) //check if height is lower than max height - threshold
    {
        apogee_counter++;
        if (
          apogee_counter >
          APOGEE_HEIGHT_COUNT) //check if height is lower than max height - threshold for 10 consecutive readings -> Apogee detected
        {
            apogee_timestamp_ms =
              esp_timer_get_time() / 1000; //take a timestamp for the apogee time, needed for main deployment timing
#if DUAL_DEPLOYMENT
            bean_core_goto_state(FLIGHT_STATE_DROGUE_OUT);
#else
            bean_core_goto_state(FLIGHT_STATE_MAIN_OUT);
#endif
            return ESP_OK;
        }
    }
    else
    {
        apogee_counter = 0;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);

    return ESP_OK;
}

esp_err_t bean_core_process_armed(bean_context_t *ctx)
{
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
            takeoff_timestamp_ms =
              esp_timer_get_time() / 1000; //take a timestamp for the takeoff time, needed for apogee detection
            bean_core_goto_state(FLIGHT_STATE_ASCENT);
            return ESP_OK;
        }
    }
    else
    {
        launch_counter = 0;
    }

    return ESP_OK;
}

esp_err_t bean_core_process_prelaunch(bean_context_t *ctx)
{
    //chill out and wait for launch, could be temporarily a timer based wait to go to armed
    vTaskDelay(pdMS_TO_TICKS(10000)); // wait for 10 seconds
    bean_core_goto_state(FLIGHT_STATE_ARMED);

    // Log initial value
    current_height = calculate_height(bean_altimeter_get_pressure(), reference_temperature, reference_pressure);
    log_measurements(ctx);

    return ESP_OK;
}

float calculate_acceleration_vector_magnitude(float x, float y, float z)
{
    return sqrt(x * x + y * y + z * z);
}

double calculate_height(double pressure, double ref_temperature, double ref_pressure)
{ // calculate height from pressure, reference temperature and reference pressure at ground level
    return (ref_temperature / -0.0065) * (pow((pressure / ref_pressure), 0.190263) - 1.0);
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

    log_data.measurement_type = MEASUREMENT_TYPE_ALTITUDE;
    snprintf(log_data.measurement_value, sizeof(log_data.measurement_value), "%.2f", current_height);
    xQueueSend(ctx->data_log_queue, &log_data, 1);

    return ESP_OK;
}

void core_task(void *arg)
{
    bean_context_t *ctx = (bean_context_t *)arg;
    while (1)
    {
        //int64_t current_time = esp_timer_get_time();
        bean_imu_update_accel();
        if (ctx->is_metrcis_enabled) log_measurements(ctx);

        //bean_altimeter_update();
        //uint64_t elapsed_time = esp_timer_get_time() - current_time;
        //ESP_LOGI(TAG, "IMU update took %llu us", elapsed_time);
        switch (current_flight_state)
        {
        case FLIGHT_STATE_PRELAUNCH:
            bean_core_process_prelaunch(ctx);
            break;
        case FLIGHT_STATE_ARMED:
            bean_core_process_armed(ctx);
            break;
        case FLIGHT_STATE_ASCENT:
            bean_core_process_ascent(ctx);
            break;
        case FLIGHT_STATE_DROGUE_OUT:
            bean_core_process_drogue_out(ctx);
            break;
        case FLIGHT_STATE_MAIN_OUT:
            bean_core_process_main_out(ctx);
            break;
        case FLIGHT_STATE_LANDED:
            bean_core_process_landed(ctx);
            break;
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // run every 10ms // about 1 tick
    }
}

esp_err_t bean_core_init(bean_context_t *ctx)
{
    ESP_LOGI(TAG, "Initializing Bean Core");
    bean_altimeter_update(); //initial update to get reference pressure and temperature
    reference_pressure    = bean_altimeter_get_pressure(); //initial reference pressure and temperature
    reference_temperature = bean_altimeter_get_temperature() + 273.15;
    ESP_LOGI(TAG, "Reference pressure: %f Pa", reference_pressure);
    ESP_LOGI(TAG, "Reference temperature: %f K", reference_temperature);
    xTaskCreate(&core_task, "core_task", 1024 * 4, (void *)ctx, tskIDLE_PRIORITY, &core_task_handle);

    if (core_task_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create core task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
