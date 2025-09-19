#include <stdio.h>
#include "bean_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bean_altimeter.h"
#include "bean_imu.h"
#include "bean_led.h"

const static char TAG[] = "BEAN_CORE";

static TaskHandle_t core_task_handle; // handle of the core task



void core_task(void *arg)
{
    bean_context_t *ctx = (bean_context_t *)arg;
    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 50, 0 });
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 0, 0 });

        if (bean_altimeter_update() == ESP_OK)
        {
            ESP_LOGI(TAG,
                     "Pressure: %.2f Pa, Temperature: %.2f C",
                     bean_altimeter_get_pressure(),
                     bean_altimeter_get_temperature());
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read from altimeter");
        }
        if (bean_imu_update_accel() == ESP_OK)
        {
            ESP_LOGI(TAG,
                     "Accel X: %.2f m/s^2, Y: %.2f m/s^2, Z: %.2f m/s^2",
                     get_x_accel_data(),
                     get_y_accel_data(),
                     get_z_accel_data());
        }
        else
        {
            ESP_LOGE(TAG, "Failed to update accelerometer data");
        }
        if (bean_imu_update_gyro() == ESP_OK)
        {
            ESP_LOGI(TAG,
                     "Gyro X: %.2f rad/s, Y: %.2f rad/s, Z: %.2f rad/s",
                     get_x_gyro_data(),
                     get_y_gyro_data(),
                     get_z_gyro_data());
        }
        else
        {
            ESP_LOGE(TAG, "Failed to update gyroscope data");
        }
    }
}


esp_err_t bean_core_init(bean_context_t *ctx)
{
    ESP_LOGI(TAG, "Initializing Bean Core");
    xTaskCreate(&core_task, "core_task", 1024*4, (void *)ctx, tskIDLE_PRIORITY, &core_task_handle);
    if (core_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create core task");
        return ESP_FAIL;
    }
    return ESP_OK;
}


