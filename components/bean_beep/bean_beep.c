#include <stdio.h>
#include "bean_beep.h"
#include "driver/mcpwm.h"
#include "bean_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_check.h"

static char tag[] = "beep";

#define BEEP_MCPWM_UNIT      MCPWM_UNIT_0
#define BEEP_MCPWM_TIMER     MCPWM_TIMER_0

// Global variables for non-blocking operation
static TimerHandle_t beep_timer = NULL;
static bool beep_playing = false;

// Timer callback to stop beep
static void beep_timer_callback(TimerHandle_t timer)
{
    // Silence the buzzer
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, 0.0);
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, 0.0);
    
    beep_playing = false;
    ESP_LOGI(tag, "Beep finished");
}

esp_err_t bean_beep_init()
{
    esp_err_t ret = ESP_OK;
    
    // Initialize MCPWM GPIOs
    mcpwm_gpio_init(BEEP_MCPWM_UNIT, MCPWM0A, PIN_BUZ_P);
    mcpwm_gpio_init(BEEP_MCPWM_UNIT, MCPWM0B, PIN_BUZ_N);

    // Configure MCPWM timer for default 2kHz, 50% duty
    mcpwm_config_t pwm_config = {
        .frequency = 2000,
        .cmpr_a = 0.0,
        .cmpr_b = 0.0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_COUNTER,
    };
    ret = mcpwm_init(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, &pwm_config);
    ESP_RETURN_ON_ERROR(ret, tag, "Failed to initialize MCPWM: %s", esp_err_to_name(ret));

    // Set complementary output: A = PWM, B = inverted PWM
    mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, MCPWM_DUTY_MODE_1); // Inverted
    
    // Create timer for automatic beep shutdown
    beep_timer = xTimerCreate(
        "beep_timer",
        pdMS_TO_TICKS(100),  // Default 100ms, will be adjusted
        pdFALSE,             // One-shot timer
        NULL,                // Timer ID
        beep_timer_callback
    );
    if (beep_timer == NULL) {
        ESP_LOGE(tag, "Failed to create beep timer");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(tag, "Beep component initialized successfully");
    return ESP_OK;
}

// Non-blocking beep function
esp_err_t bean_beep_sound(uint32_t frequency, uint32_t duration_ms)
{
    if (duration_ms == 0) {
        ESP_LOGE(tag, "Invalid duration: %d ms", duration_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop any currently playing beep
    if (beep_playing) {
        bean_beep_stop();
    }
    
    // Set frequency and start beep
    mcpwm_set_frequency(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, frequency);
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, 50.0);
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, 50.0);
    mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, MCPWM_DUTY_MODE_1); // Inverted
    
    beep_playing = true;
    
    // Start timer to stop beep after duration
    xTimerChangePeriod(beep_timer, pdMS_TO_TICKS(duration_ms), 0);
    xTimerStart(beep_timer, 0);
    
    ESP_LOGI(tag, "Beep started: %d Hz for %d ms", frequency, duration_ms);
    return ESP_OK;
}

// Stop current beep
esp_err_t bean_beep_stop(void)
{
    // Stop timer
    xTimerStop(beep_timer, 0);
    
    // Silence the buzzer
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, 0.0);
    mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, 0.0);
    
    beep_playing = false;
    
    ESP_LOGI(tag, "Beep stopped");
    return ESP_OK;
}

// Check if beep is currently playing
bool bean_beep_is_playing(void)
{
    return beep_playing;
}
