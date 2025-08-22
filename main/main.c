#include <stdio.h>
#include <esp_log.h>
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <freertos/task.h>
#include "systemio.h"
#include "sdkconfig.h"
#include "altimeter.h"
#include "bean_storage.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include <string.h>
#include "driver/uart.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include "nvs_flash.h"
#include "leds.h"
#include "systemio.h"
#include "bmi088.h"


static char TAG[] = "MAIN";


void app_main()
{   
    ESP_LOGI(TAG, "Starting up...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    io_init();
    
    leds_set_color(0, (led_color_rgb_t){255, 255, 255});
    leds_set_color(1, (led_color_rgb_t){255, 255, 255});
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    leds_set_color(0, (led_color_rgb_t){0, 0, 0});
    leds_set_color(1, (led_color_rgb_t){0, 0, 0});

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        leds_set_color(0, (led_color_rgb_t){0, 0, 255});
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        leds_set_color(0, (led_color_rgb_t){0, 0, 0});
    }
}

