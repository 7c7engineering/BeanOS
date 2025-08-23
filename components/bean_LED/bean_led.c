#include "bean_led.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "gamma.h"

static char tag[] = "leds";

static const gpio_num_t PIN_LEDR1      = GPIO_NUM_33;
static const gpio_num_t PIN_LEDG1      = GPIO_NUM_34;
static const gpio_num_t PIN_LEDB1      = GPIO_NUM_35;
static const gpio_num_t PIN_LEDG2      = GPIO_NUM_16;
static const gpio_num_t PIN_LEDB2      = GPIO_NUM_17;
static const gpio_num_t PIN_LEDR2      = GPIO_NUM_18;

const ledc_channel_t LEDC_CHANNEL_RED1 = LEDC_CHANNEL_0;
const ledc_channel_t LEDC_CHANNEL_GREEN1 = LEDC_CHANNEL_1;
const ledc_channel_t LEDC_CHANNEL_BLUE1 = LEDC_CHANNEL_2;
const ledc_channel_t LEDC_CHANNEL_RED2 = LEDC_CHANNEL_3;
const ledc_channel_t LEDC_CHANNEL_GREEN2 = LEDC_CHANNEL_4;
const ledc_channel_t LEDC_CHANNEL_BLUE2 = LEDC_CHANNEL_5;


esp_err_t leds_init() {
    esp_err_t ret = ESP_OK;

    ledc_timer_config_t ledc_timer = {
        .speed_mode         = LEDC_LOW_SPEED_MODE,
        .duty_resolution    = LEDC_TIMER_13_BIT,
        .timer_num          = LEDC_TIMER_0,
        .freq_hz            = 1000,
        .clk_cfg            = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC timer: %s", esp_err_to_name(ret));

    ledc_channel_config_t ledc_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = PIN_LEDR1,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_conf.flags.output_invert = 1;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 0: %s", esp_err_to_name(ret));
    ledc_conf.channel = LEDC_CHANNEL_1;
    ledc_conf.gpio_num = PIN_LEDG1;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 1: %s", esp_err_to_name(ret));
    ledc_conf.channel = LEDC_CHANNEL_2;
    ledc_conf.gpio_num = PIN_LEDB1;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 2: %s", esp_err_to_name(ret));
    ledc_conf.channel = LEDC_CHANNEL_3;
    ledc_conf.gpio_num = PIN_LEDR2;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 3: %s", esp_err_to_name(ret));    
    ledc_conf.channel = LEDC_CHANNEL_4;
    ledc_conf.gpio_num = PIN_LEDG2;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 4: %s", esp_err_to_name(ret));
    ledc_conf.channel = LEDC_CHANNEL_5;
    ledc_conf.gpio_num = PIN_LEDB2;
    ret = ledc_channel_config(&ledc_conf);
    ESP_RETURN_ON_ERROR(ret, tag, "Error configuring LEDC channel 5: %s", esp_err_to_name(ret));
    
    return ESP_OK;
}

esp_err_t leds_set_color(uint8_t led, led_color_rgb_t color){
    ledc_channel_t r_channel, g_channel, b_channel;
    switch(led){
        case 0:
            r_channel = LEDC_CHANNEL_RED1;
            g_channel = LEDC_CHANNEL_GREEN1;
            b_channel = LEDC_CHANNEL_BLUE1;
            break;
        case 1:
            r_channel = LEDC_CHANNEL_RED2;
            g_channel = LEDC_CHANNEL_GREEN2;
            b_channel = LEDC_CHANNEL_BLUE2;
            break;
        default:
            ESP_LOGE(tag, "Invalid LED number: %d", led);
            return ESP_ERR_INVALID_ARG;
    }
    uint16_t r = (led_gamma_r[color.r] >> 3);
    uint16_t g = (led_gamma_g[color.g] >> 3);
    uint16_t b = (led_gamma_b[color.b] >> 3);

    //ESP_LOGI(tag, "Setting LED %d to RGB(%d, %d, %d)", led, r, g, b);

    esp_err_t ret = ESP_OK;
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, r_channel, r);
    ESP_RETURN_ON_ERROR(ret, tag, "Error setting LEDC duty for red channel: %s", esp_err_to_name(ret));
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, g_channel, g);
    ESP_RETURN_ON_ERROR(ret, tag, "Error setting LEDC duty for green channel: %s", esp_err_to_name(ret));
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, b_channel, b);
    ESP_RETURN_ON_ERROR(ret, tag, "Error setting LEDC duty for blue channel: %s", esp_err_to_name(ret));
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, r_channel);
    ESP_RETURN_ON_ERROR(ret, tag, "Error updating LEDC duty for red channel: %s", esp_err_to_name(ret));
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, g_channel);
    ESP_RETURN_ON_ERROR(ret, tag, "Error updating LEDC duty for green channel: %s", esp_err_to_name(ret));
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, b_channel);
    ESP_RETURN_ON_ERROR(ret, tag, "Error updating LEDC duty for blue channel: %s", esp_err_to_name(ret));
    return ret;
}