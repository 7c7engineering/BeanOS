#pragma once
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include "string.h"
#include "rom/ets_sys.h"
#include "bmp3.h"
#include <esp_log.h>

#define BMP3_DOUBLE_PRECISION_COMPENSATION

esp_err_t bean_altimeter_init(void);
esp_err_t bean_altimeter_update(void);
double bean_altimeter_get_pressure(void);
double bean_altimeter_get_temperature(void);
esp_err_t setTemperatureOversampling(uint8_t oversample);
esp_err_t setPressureOversampling(uint8_t oversample);
esp_err_t setIIRFilterCoeff(uint8_t coeff);
esp_err_t setOutputDataRate(uint8_t odr);
