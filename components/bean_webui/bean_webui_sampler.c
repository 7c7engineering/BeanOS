#include <stdio.h>
#include "bean_webui_sampler.h"
#include "bean_webui_ws.h"
#include "bean_altimeter.h"
#include "bean_battery.h"
#include "bean_bits.h"
#include "bean_imu.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BEAN_WEBUI_SAMPLER";

static TaskHandle_t sampler_task_handle = NULL;
static bean_context_t *bean_ctx;
static uint32_t period_ms;

static void vtask_webui_sampler(void *pvParameter)
{
    static char msg[256];
    TickType_t last_wake = xTaskGetTickCount();
    bool sensors_ok      = true;

    while (1)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));

        esp_err_t err = bean_altimeter_update();
        err |= bean_imu_update_accel();
        err |= bean_imu_update_gyro();
        // Log on state changes only, an I2C hiccup at 5 Hz would flood the console
        if ((err == ESP_OK) != sensors_ok)
        {
            sensors_ok = (err == ESP_OK);
            if (sensors_ok)
                ESP_LOGI(TAG, "Sensor reads recovered");
            else
                ESP_LOGW(TAG, "Sensor read failed, streaming stale values");
        }

        static bool had_clients = false;
        bool has_clients        = bean_webui_ws_has_clients();
        if (has_clients != had_clients)
        {
            had_clients = has_clients;
            ESP_LOGI(TAG, "%s", has_clients ? "Client connected, streaming telemetry" : "No clients, telemetry paused");
        }
        if (!has_clients)
            continue;

        EventBits_t bits = xEventGroupGetBits(bean_ctx->system_event_group);
        bool armed       = (bits & BEAN_SYSTEM_ARMED) != 0;
        int len          = snprintf(msg,
                           sizeof(msg),
                           "{\"t\":%lu,\"p\":%.2f,\"tc\":%.2f,"
                                    "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
                                    "\"gx\":%.3f,\"gy\":%.3f,\"gz\":%.3f,"
                                    "\"vbat\":%d,\"armed\":%s,\"state\":\"%s\"}",
                           esp_log_timestamp(),
                           bean_altimeter_get_pressure(),
                           bean_altimeter_get_temperature(),
                           get_x_accel_data(),
                           get_y_accel_data(),
                           get_z_accel_data(),
                           get_x_gyro_data(),
                           get_y_gyro_data(),
                           get_z_gyro_data(),
                           bean_battery_get_voltage_mv(),
                           armed ? "true" : "false",
                           armed ? "armed" : "pre_launch");
        if (len > 0 && len < sizeof(msg))
            bean_webui_ws_broadcast(msg, len);
    }
}

esp_err_t bean_webui_sampler_start(bean_context_t *ctx, int update_hz)
{
    if (update_hz < 1)
        update_hz = 1;
    if (update_hz > 10)
        update_hz = 10;

    bean_ctx  = ctx;
    period_ms = 1000 / update_hz;

    xTaskCreate(&vtask_webui_sampler, "webui_sampler", 4096, NULL, tskIDLE_PRIORITY + 1, &sampler_task_handle);
    if (sampler_task_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create sampler task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sampling sensors at %d Hz", update_hz);
    return ESP_OK;
}

void bean_webui_sampler_stop(void)
{
    if (sampler_task_handle != NULL)
    {
        vTaskDelete(sampler_task_handle);
        sampler_task_handle = NULL;
    }
}
