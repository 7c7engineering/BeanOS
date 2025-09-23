#include "bean_pyro.h"
#include "bean_context.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static char tag[] = "pyro";

// Pyro channel pin definitions
static const gpio_num_t pyro_pins[] = {
    PIN_PYRO_0, // GPIO 38
    PIN_PYRO_1, // GPIO 37
    PIN_PYRO_2, // GPIO 14
    PIN_PYRO_3 // GPIO 15
};

#define PYRO_CHANNEL_COUNT 4

// Pyro channel status tracking
static pyro_channel_status_t channel_status[PYRO_CHANNEL_COUNT] = { 0 };

// Timer handles for each channel
static TimerHandle_t fire_timers[PYRO_CHANNEL_COUNT] = { NULL };

// Timer callback function to turn off a pyro channel
static void pyro_timer_callback(TimerHandle_t timer)
{
    uint32_t channel = (uint32_t)pvTimerGetTimerID(timer);
    if (channel < PYRO_CHANNEL_COUNT)
    {
        // Turn off the pyro channel
        gpio_set_level(pyro_pins[channel], 0);
        ESP_LOGI(tag, "Pyro channel %d turned off after %d ms", channel, channel_status[channel].duration);
    }
}

esp_err_t bean_pyro_init(void)
{
    esp_err_t ret = ESP_OK;

    // Configure GPIO pins for pyro channels
    gpio_config_t io_conf = { .intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_OUTPUT, .pull_down_en = 0, .pull_up_en = 0, .pin_bit_mask = 0 };

    // Set pin bit mask for all pyro pins
    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        io_conf.pin_bit_mask |= (1ULL << pyro_pins[i]);
    }

    ret = gpio_config(&io_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring pyro GPIO pins: %s", esp_err_to_name(ret));

    // Initialize all channels to off state
    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        gpio_set_level(pyro_pins[i], 0);
        channel_status[i].fired     = false;
        channel_status[i].fire_time = 0;
        channel_status[i].duration  = 0;

        // Create timer for this channel
        fire_timers[i] = xTimerCreate("pyro_timer",
                                      pdMS_TO_TICKS(100), // Default 100ms, will be adjusted when firing
                                      pdFALSE, // One-shot timer
                                      (void *)i, // Timer ID (channel number)
                                      pyro_timer_callback);

        if (fire_timers[i] == NULL)
        {
            ESP_LOGE(tag, "Failed to create timer for pyro channel %d", i);
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(tag, "Pyro component initialized successfully");
    return ESP_OK;
}

esp_err_t bean_pyro_fire(pyro_channel_t channel, uint32_t duration_ms)
{
    if (channel >= PYRO_CHANNEL_COUNT)
    {
        ESP_LOGE(tag, "Invalid pyro channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (duration_ms == 0)
    {
        ESP_LOGE(tag, "Invalid duration: %d ms", duration_ms);
        return ESP_ERR_INVALID_ARG;
    }

    // Check if channel is already fired
    if (channel_status[channel].fired)
    {
        ESP_LOGW(tag, "Pyro channel %d already fired", channel);
        return ESP_ERR_INVALID_STATE;
    }

    // Stop existing timer if running
    xTimerStop(fire_timers[channel], 0);

    // Fire the pyro channel
    gpio_set_level(pyro_pins[channel], 1);

    // Update status
    channel_status[channel].fired     = true;
    channel_status[channel].fire_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    channel_status[channel].duration  = duration_ms;

    // Start timer to turn off the channel
    if (xTimerChangePeriod(fire_timers[channel], pdMS_TO_TICKS(duration_ms), 0) != pdPASS)
    {
        ESP_LOGE(tag, "Failed to start timer for pyro channel %d", channel);
        // Turn off immediately if timer fails
        gpio_set_level(pyro_pins[channel], 0);
        channel_status[channel].fired = false;
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(tag, "Pyro channel %d fired for %d ms", channel, duration_ms);
    return ESP_OK;
}

esp_err_t bean_pyro_fire_all(uint32_t duration_ms)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        ret = bean_pyro_fire((pyro_channel_t)i, duration_ms);
        if (ret != ESP_OK)
        {
            ESP_LOGE(tag, "Failed to fire pyro channel %d", i);
            // Continue trying other channels
        }
    }

    return ret;
}

bool bean_pyro_is_fired(pyro_channel_t channel)
{
    if (channel >= PYRO_CHANNEL_COUNT)
    {
        return false;
    }
    return channel_status[channel].fired;
}

esp_err_t bean_pyro_get_channel_status(pyro_channel_t channel, pyro_channel_status_t *status)
{
    if (channel >= PYRO_CHANNEL_COUNT || status == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *status = channel_status[channel];
    return ESP_OK;
}

esp_err_t bean_pyro_get_all_status(pyro_channel_status_t *status)
{
    if (status == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        status[i] = channel_status[i];
    }

    return ESP_OK;
}

esp_err_t bean_pyro_reset_channel(pyro_channel_t channel)
{
    if (channel >= PYRO_CHANNEL_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Stop timer if running
    xTimerStop(fire_timers[channel], 0);

    // Turn off the channel
    gpio_set_level(pyro_pins[channel], 0);

    // Reset status
    channel_status[channel].fired     = false;
    channel_status[channel].fire_time = 0;
    channel_status[channel].duration  = 0;

    ESP_LOGI(tag, "Pyro channel %d reset", channel);
    return ESP_OK;
}

esp_err_t bean_pyro_reset_all(void)
{
    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        esp_err_t ret = bean_pyro_reset_channel((pyro_channel_t)i);
        if (ret != ESP_OK)
        {
            ESP_LOGE(tag, "Failed to reset pyro channel %d", i);
            // Continue with other channels
        }
    }

    ESP_LOGI(tag, "All pyro channels reset");
    return ESP_OK;
}

uint8_t bean_pyro_get_fired_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < PYRO_CHANNEL_COUNT; i++)
    {
        if (channel_status[i].fired)
        {
            count++;
        }
    }
    return count;
}
