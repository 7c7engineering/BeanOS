#pragma once
#include "esp_err.h"

esp_err_t bean_beep_init();
esp_err_t bean_beep_sound(uint32_t frequency, uint32_t duration_ms);
