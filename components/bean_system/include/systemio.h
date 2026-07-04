#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "driver/i2c_master.h"

esp_err_t io_init();
esp_err_t io_get_i2c_bus(i2c_master_bus_handle_t *out_bus);