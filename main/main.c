#include <stdio.h>
#include <esp_log.h>
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <freertos/task.h>
#include "systemio.h"
#include "bean_altimeter.h"
#include "bean_storage.h"
#include <string.h>
#include "nvs_flash.h"
#include "bean_led.h"
#include "bean_imu.h"
#include "bean_beep.h"
#include "bean_storage.h"
#include "bean_battery.h"
#include "bean_context.h"
#include "bean_webui.h"
#include "hal/usb_serial_jtag_ll.h"
#include "cJSON.h"

static char TAG[] = "MAIN";

static bean_context_t *bean_context = NULL; // The main bean context that is shared between components

// Survives soft resets (not power cycles): set when USB MSC failed so the next
// boot skips MSC instead of retrying it into a reboot loop
RTC_NOINIT_ATTR static uint32_t msc_failed_marker;
#define MSC_FAILED_MAGIC 0xB5C0FA11u

esp_err_t bean_init()
{
    ESP_RETURN_ON_ERROR(bean_context_init(&bean_context), TAG, "Bean Context Init failed");
    ESP_RETURN_ON_ERROR(io_init(), TAG, "IO Init failed");
    ESP_RETURN_ON_ERROR(bean_storage_init(bean_context), TAG, "Storage Init failed");
    ESP_RETURN_ON_ERROR(bean_led_init(), TAG, "LEDs Init failed");
    ESP_RETURN_ON_ERROR(bean_battery_init(bean_context), TAG, "Battery Init failed");
    ESP_RETURN_ON_ERROR(bean_altimeter_init(), TAG, "BMP390 Init failed");
    ESP_RETURN_ON_ERROR(bean_imu_init(), TAG, "BMI088 Init failed");
    ESP_RETURN_ON_ERROR(bean_beep_init(), TAG, "Beep Init failed");
    return ESP_OK;
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

    // If powered by USB and not in development mode
    if (bean_battery_is_usb_powered() && !usb_serial_jtag_ll_txfifo_writable() && msc_failed_marker != MSC_FAILED_MAGIC)
    {
        bean_context->is_not_usb_msc = false;
        ESP_LOGI(TAG, "Powered by USB: switching to MSC mode");
        vTaskDelay(8000 / portTICK_PERIOD_MS);
        if (storage_enable_usb_msc() == ESP_OK)
        {
            // Prevent it from continuing in the code
            // The device should now only work as an USB MSC device
            while (1)
            {
                // Display slow flashing soft white LED
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 50, 50, 50 });
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });
            }
        }
        // A failed MSC handover leaves the filesystem in a broken state, so
        // continuing in place is not safe. Restart once; the marker makes the
        // next boot skip MSC and come up as a normal logger with the web UI.
        ESP_LOGE(TAG, "USB MSC failed to start, restarting into normal mode");
        msc_failed_marker = MSC_FAILED_MAGIC;
        esp_restart();
    }

    bean_context->is_not_usb_msc = true;

    // The web UI must never brick flight logging, so a failure only warns
    if (bean_webui_init(bean_context) != ESP_OK)
    {
        ESP_LOGW(TAG, "Web UI failed to start; continuing without it");
    }

    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 255, 255, 255 });
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){ 0, 0, 0 });

    // Sensor polling lives in the web UI sampler task; this loop is only a heartbeat
    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 50, 0 });
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bean_led_set_color(LED_L1, (led_color_rgb_t){ 0, 0, 0 });

        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }
}
