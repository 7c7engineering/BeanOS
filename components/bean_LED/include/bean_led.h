#pragma once

#include "esp_err.h"

typedef enum
{
    LED_L1   = 0,
    LED_L2   = 1,
    LED_BOTH = 2
} led_select_t;

typedef struct led_color_rgb_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_rgb_t;

typedef struct led_color_hsv_t
{
    uint8_t h;
    uint8_t s;
    uint8_t v;
} led_color_hsv_t;

esp_err_t bean_led_init();
esp_err_t bean_led_set_color(led_select_t led, led_color_rgb_t color);
