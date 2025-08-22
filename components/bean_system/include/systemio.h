#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include <driver/gpio.h>
#include "driver/i2c.h"
#include "leds.h"

esp_err_t io_init();