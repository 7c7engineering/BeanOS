#pragma once
#include "esp_types.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "bmi08x.h"
#include "rom/ets_sys.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bmi088_mm.h"

#define BMI088_ACC_I2C_ADDR BMI08_ACCEL_I2C_ADDR_PRIMARY
#define BMI088_GYR_I2C_ADDR BMI08_GYRO_I2C_ADDR_PRIMARY

/**
 * @brief Initializes the BMI088 sensor.
 *
 * @return esp_err_t Returns ESP_OK if the initialization is successful, otherwise an error code.
 */
esp_err_t bean_imu_init();

/**
 * @brief Sets the output data rate (ODR) for the accelerometer.
 *
 * @param odr The desired output data rate.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_accel_odr(uint8_t odr);

/**
 * @brief Sets the range for the accelerometer.
 *
 * @param range The desired range.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_accel_range(uint8_t range);

/**
 * @brief Sets the bandwidth (BW) for the accelerometer.
 *
 * @param bw The desired bandwidth.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_accel_bw(uint8_t bw);

/**
 * @brief Sets the power mode for the accelerometer.
 *
 * @param power_mode The desired power mode.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_accel_power_mode(uint8_t power_mode);

/**
 * @brief Sets the output data rate (ODR) for the gyroscope.
 *
 * @param odr The desired output data rate.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_gyro_odr(uint8_t odr);

/**
 * @brief Sets the range for the gyroscope.
 *
 * @param range The desired range.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_gyro_range(uint8_t range);

/**
 * @brief Sets the bandwidth (BW) for the gyroscope.
 *
 * @param bw The desired bandwidth.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_gyro_bw(uint8_t bw);

/**
 * @brief Sets the power mode for the gyroscope.
 *
 * @param power_mode The desired power mode.
 * @return esp_err_t Returns ESP_OK if the setting is successful, otherwise an error code.
 */
esp_err_t set_gyro_power_mode(uint8_t power_mode);

/**
 * @brief Updates the accelerometer data.
 *
 * @return esp_err_t Returns ESP_OK if the update is successful, otherwise an error code.
 */
esp_err_t bean_imu_update_accel();

/**
 * @brief Updates the gyroscope data.
 *
 * @return esp_err_t Returns ESP_OK if the update is successful, otherwise an error code.
 */
esp_err_t bean_imu_update_gyro();

/**
 * @brief Gets the X-axis accelerometer data.
 *
 * @return float The X-axis accelerometer data.
 */
float get_x_accel_data();

/**
 * @brief Gets the Y-axis accelerometer data.
 *
 * @return float The Y-axis accelerometer data.
 */
float get_y_accel_data();

/**
 * @brief Gets the Z-axis accelerometer data.
 *
 * @return float The Z-axis accelerometer data.
 */
float get_z_accel_data();

/**
 * @brief Gets the X-axis gyroscope data.
 *
 * @return float The X-axis gyroscope data.
 */
float get_x_gyro_data();

/**
 * @brief Gets the Y-axis gyroscope data.
 *
 * @return float The Y-axis gyroscope data.
 */
float get_y_gyro_data();

/**
 * @brief Gets the Z-axis gyroscope data.
 *
 * @return float The Z-axis gyroscope data.
 */
float get_z_gyro_data();
