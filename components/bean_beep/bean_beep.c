#include <stdio.h>
#include "bean_beep.h"
#include "driver/ledc.h"
#include "bean_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const ledc_mode_t BEEP_LEDC_MODE          = LEDC_LOW_SPEED_MODE;
static const ledc_timer_t BEEP_LEDC_TIMER        = LEDC_TIMER_1;
static const ledc_channel_t BEEP_LEDC_CHANNEL_P  = LEDC_CHANNEL_6;
static const ledc_channel_t BEEP_LEDC_CHANNEL_N  = LEDC_CHANNEL_7;
static const ledc_timer_bit_t BEEP_DUTY_RES_BITS = LEDC_TIMER_10_BIT;
static bool beep_initialized                      = false;

esp_err_t bean_beep_init()
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = BEEP_LEDC_MODE,
        .duty_resolution = BEEP_DUTY_RES_BITS,
        .timer_num       = BEEP_LEDC_TIMER,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK)
        return err;

    ledc_channel_config_t ch_cfg = {
        .speed_mode = BEEP_LEDC_MODE,
        .timer_sel  = BEEP_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
    };

    ch_cfg.channel  = BEEP_LEDC_CHANNEL_P;
    ch_cfg.gpio_num = PIN_BUZ_P;
    ch_cfg.flags.output_invert = 0;
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK)
        return err;

    ch_cfg.channel  = BEEP_LEDC_CHANNEL_N;
    ch_cfg.gpio_num = PIN_BUZ_N;
    ch_cfg.flags.output_invert = 1;
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK)
        return err;

    beep_initialized = true;
    return ESP_OK;
}

esp_err_t bean_beep_sound(uint32_t frequency, uint32_t duration)
{
    if (!beep_initialized)
    {
        esp_err_t init_err = bean_beep_init();
        if (init_err != ESP_OK)
            return init_err;
    }

    if (frequency > 0)
    {
        ledc_set_freq(BEEP_LEDC_MODE, BEEP_LEDC_TIMER, frequency);
    }

    uint32_t duty_50 = (1U << BEEP_DUTY_RES_BITS) / 2U;
    ledc_set_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_P, duty_50);
    ledc_set_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_N, duty_50);
    ledc_update_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_P);
    ledc_update_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_N);

    // Wait for duration
    vTaskDelay(duration / portTICK_PERIOD_MS);

    // Set duty to 0 to silence
    ledc_set_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_P, 0);
    ledc_set_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_N, 0);
    ledc_update_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_P);
    ledc_update_duty(BEEP_LEDC_MODE, BEEP_LEDC_CHANNEL_N);
    return ESP_OK;
}
