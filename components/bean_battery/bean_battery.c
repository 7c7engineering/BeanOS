#include <stdio.h>
#include "esp_log.h"
#include "esp_check.h"
#include "bean_battery.h"
#include "bean_context.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


const char TAG[] = "bean_battery";

const float resistor_voltage_divider = 2.0f; // Could later be part of the configuration.

void vtask_battery_monitor(void *pvParameter);

static adc_channel_t vbat_adc_channel;
static adc_unit_t vbat_adc_unit;
static adc_oneshot_unit_handle_t vbat_adc_handle;
static adc_cali_handle_t vbat_adc_cali_handle;

TaskHandle_t battery_monitor_task_handle;

esp_err_t bean_battery_init(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_direction(PIN_USB_DET, GPIO_MODE_INPUT), TAG, "Set USB DET pin direction failed");
    ESP_RETURN_ON_ERROR(gpio_set_direction(PIN_CHRG_STAT, GPIO_MODE_INPUT), TAG, "Set CHRG STAT pin direction failed");
    ESP_RETURN_ON_ERROR(gpio_set_direction(PIN_VBAT_ADC, GPIO_MODE_INPUT), TAG, "Set VBAT ADC pin direction failed");

    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(PIN_VBAT_ADC, &vbat_adc_unit, &vbat_adc_channel), TAG, "VBAT I/O pin is not a valid ADC channel");

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = vbat_adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config1, &vbat_adc_handle), TAG, "Failed to create VBAT ADC unit");

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_WIDTH_BIT_12,
        .atten = ADC_ATTEN_DB_11};
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(vbat_adc_handle, vbat_adc_channel, &chan_config), TAG, "Failed to configure VBAT ADC channel");
    
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = vbat_adc_unit,
        .chan = vbat_adc_channel,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_WIDTH_BIT_12,
    };
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_config, &vbat_adc_cali_handle), TAG, "Failed to create VBAT ADC calibration handle");

    xTaskCreate(&vtask_battery_monitor, "battery_monitor", 2048, NULL, tskIDLE_PRIORITY, &battery_monitor_task_handle);
    if (battery_monitor_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create battery monitor task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void vtask_battery_monitor(void *pvParameter)
{
    ESP_LOGI(TAG, "Battery monitor task started");
    int voltage_raw, voltage_mv = 0;
    while (1)
    {  
        esp_err_t ret = adc_oneshot_read(vbat_adc_handle, vbat_adc_channel, &voltage_raw);
        if(ret == ESP_OK)
        {
            adc_cali_raw_to_voltage(vbat_adc_cali_handle, voltage_raw, &voltage_mv);
            voltage_mv *= resistor_voltage_divider;
            ESP_LOGI(TAG, "Battery Voltage: %d mV", voltage_mv);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read battery voltage!");
        }

        // Read the USB detection pin
        int usb_detected = gpio_get_level(PIN_USB_DET);
        ESP_LOGI(TAG, "USB detected: %d", usb_detected);
        int chrg_stat = gpio_get_level(PIN_CHRG_STAT);
        ESP_LOGI(TAG, "Charge status: %d", chrg_stat);

        // Delay before the next reading
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}