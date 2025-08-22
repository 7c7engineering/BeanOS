#pragma once
#include "esp_check.h"
#include "esp_log.h"
#include "pins.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "gamma.h"

typedef struct led_color_rgb_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_rgb_t;

typedef struct led_color_hsv_t {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} led_color_hsv_t;

esp_err_t leds_init(void);
esp_err_t leds_set_color(uint8_t led, led_color_rgb_t color);
//esp_err_t leds_set_color_hsv(uint8_t led, led_color_hsv_t color);

