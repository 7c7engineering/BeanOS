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
#include "bean_battery.h"
#include "bean_context.h"
#include "bean_core.h"
#include "esp_timer.h"
#include "pins.h"
#include "driver/gpio.h"

static char TAG[] = "MAIN";

static bean_context_t *bean_context = NULL; // The main bean context that is shared between components

esp_err_t bean_init()
{
    ESP_RETURN_ON_ERROR(bean_context_init(&bean_context),   TAG, "Bean Context Init failed");
    ESP_RETURN_ON_ERROR(io_init(),                          TAG, "IO Init failed");
    ESP_RETURN_ON_ERROR(bean_led_init(),                    TAG, "LEDs Init failed");
    ESP_RETURN_ON_ERROR(bean_battery_init(bean_context),    TAG, "Battery Init failed");
    ESP_RETURN_ON_ERROR(bean_altimeter_init(),              TAG, "BMP390 Init failed");
    ESP_RETURN_ON_ERROR(bean_imu_init(),                    TAG, "BMI088 Init failed");
    ESP_RETURN_ON_ERROR(bean_beep_init(),                   TAG, "Beep Init failed");
    ESP_RETURN_ON_ERROR(bean_storage_init(),                TAG, "Storage Init failed");
    ESP_RETURN_ON_ERROR(bean_core_init(bean_context),       TAG, "Core Init failed");
    return ESP_OK;
}

void test_pyro_channels()
{
    // Set PYRO channels as output and low
    gpio_set_direction(PIN_PYRO_0, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_PYRO_1, GPIO_MODE_OUTPUT);       
    gpio_set_level(PIN_PYRO_0, 0);
    gpio_set_level(PIN_PYRO_1, 0);
    for (int i = 0; i < 30; i++)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 0, 0 });
        bean_beep_sound(NOTE_A5, 100);
        bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });
    }
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 255, 0 });
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bean_beep_sound(NOTE_B5, 100);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bean_beep_sound(NOTE_C5, 100);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bean_beep_sound(NOTE_E5, 100);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    for (int i = 0; i < 5; i++)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 255, 255 });
        bean_beep_sound(NOTE_A6, 50);
        bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });
    }
    // Set pyro channel high
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 255, 255 });
    gpio_set_level(PIN_PYRO_0, 1);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_PYRO_0, 0);
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 255 });

    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}


void app_main()
{
    ESP_LOGI(TAG, "Starting up...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    if (bean_init() != ESP_OK)
    {
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

    //test_pyro_channels();

    while (1)
    {

        vTaskDelay(10000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Uptime: %lld seconds", esp_timer_get_time() / 1000000);
        
    }
}
