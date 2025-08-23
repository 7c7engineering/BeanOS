#pragma once
#include "soc/gpio_num.h"

// GPIO pins
// static const gpio_num_t PIN_VBAT_ADC   = GPIO_NUM_1;
// static const gpio_num_t PIN_USB_DET    = GPIO_NUM_6;
// static const gpio_num_t PIN_BUZ_P      = GPIO_NUM_7;
// static const gpio_num_t PIN_BUZ_N      = GPIO_NUM_8;
// static const gpio_num_t PIN_PYRO_2     = GPIO_NUM_14;
// static const gpio_num_t PIN_PYRO_3     = GPIO_NUM_15;
// static const gpio_num_t PIN_LEDG2      = GPIO_NUM_16;
// static const gpio_num_t PIN_LEDB2      = GPIO_NUM_17;
// static const gpio_num_t PIN_LEDR2      = GPIO_NUM_18;
// static const gpio_num_t PIN_SERVO1     = GPIO_NUM_21;
// static const gpio_num_t PIN_LEDR1      = GPIO_NUM_33;
// static const gpio_num_t PIN_LEDG1      = GPIO_NUM_34;
// static const gpio_num_t PIN_LEDB1      = GPIO_NUM_35;
// static const gpio_num_t PIN_SERVO2     = GPIO_NUM_36;
// static const gpio_num_t PIN_PYRO_1     = GPIO_NUM_37;
// static const gpio_num_t PIN_PYRO_0     = GPIO_NUM_38;
// static const gpio_num_t PIN_I2C_SDA = GPIO_NUM_47;
// static const gpio_num_t PIN_I2C_SCL = GPIO_NUM_48;

// ADC channels
static const adc_channel_t ADC_VBAT = ADC_CHANNEL_0;

#define I2C_MASTER_SCL_IO   48    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO   47    /*!< gpio number for I2C master data  */
