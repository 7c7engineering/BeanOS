#include <stdio.h>
#include <esp_log.h>
#include "esp_check.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <freertos/task.h>
#include "systemio.h"
#include "sdkconfig.h"
#include "bean_altimeter.h"
#include "bean_storage.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include <string.h>
#include "driver/uart.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include "nvs_flash.h"
#include "bean_led.h"
#include "bean_imu.h"
#include "bean_beep.h"
#include "bean_storage.h"

static char TAG[] = "MAIN";

esp_err_t bean_init()
{
    ESP_RETURN_ON_ERROR(io_init(), TAG, "IO Init failed");
    ESP_RETURN_ON_ERROR(bean_led_init(), TAG, "LEDs Init failed");
    ESP_RETURN_ON_ERROR(bean_altimeter_init(), TAG, "BMP390 Init failed");
    ESP_RETURN_ON_ERROR(bean_imu_init(), TAG, "BMI088 Init failed");
    ESP_RETURN_ON_ERROR(bean_beep_init(), TAG, "Beep Init failed");
    ESP_RETURN_ON_ERROR(bean_storage_init(), TAG, "Storage Init failed");
    return ESP_OK;
}

void app_main()
{
    ESP_LOGI(TAG, "Starting up...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    if (bean_init() != ESP_OK) {
        ESP_LOGE(TAG, "Bean Init failed");
        bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 0, 0 });
        return;
    }

    bean_beep_sound(NOTE_C6, 100);
    bean_beep_sound(NOTE_E4, 100);
    bean_beep_sound(NOTE_G4, 100);
    bean_beep_sound(NOTE_C5, 100);
    bean_beep_sound(NOTE_E5, 100);

    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 255, 255 });
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });

    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 50, 0 });
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 0, 0 });
        if (bean_altimeter_update() == ESP_OK) {
            ESP_LOGI(TAG,
                     "Pressure: %.2f Pa, Temperature: %.2f C",
                     bean_altimeter_get_pressure(),
                     bean_altimeter_get_temperature());
        } else {
            ESP_LOGE(TAG, "Failed to read from altimeter");
        }
        if (bean_imu_update_accel() == ESP_OK) {
            ESP_LOGI(TAG,
                     "Accel X: %.2f m/s^2, Y: %.2f m/s^2, Z: %.2f m/s^2",
                     get_x_accel_data(),
                     get_y_accel_data(),
                     get_z_accel_data());
        } else {
            ESP_LOGE(TAG, "Failed to update accelerometer data");
        }
        if (bean_imu_update_gyro() == ESP_OK) {
            ESP_LOGI(TAG,
                     "Gyro X: %.2f rad/s, Y: %.2f rad/s, Z: %.2f rad/s",
                     get_x_gyro_data(),
                     get_y_gyro_data(),
                     get_z_gyro_data());
        } else {
            ESP_LOGE(TAG, "Failed to update gyroscope data");
        }
    }
}
