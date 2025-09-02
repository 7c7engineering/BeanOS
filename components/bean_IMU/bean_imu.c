/*
Author: Ricky Da Silva Marques
Date: 8/04/2024
Description: BMI088 driver for ESP32-S3 using BMI08X library from Bosch Sensortec
*/

#include "bean_imu.h"

static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width);
static BMI08_INTF_RET_TYPE i2c_write_registers(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
static BMI08_INTF_RET_TYPE i2c_read_registers(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
static void delay_us(uint32_t period, void *intf_ptr);

#define GRAVITY_EARTH (9.80665f)

static char *TAG = "BMI088";
static struct bmi08_dev *sensor;
static struct bmi08_sensor_data *accel_data;
static struct bmi08_sensor_data *gyro_data;
static struct bmi08_sensor_data_f *accel_data_f;
static struct bmi08_sensor_data_f *gyro_data_f;
const uint8_t *config_file_ptr;
uint8_t acc_dev_addr = BMI088_ACC_I2C_ADDR;
uint8_t gyr_dev_addr = BMI088_GYR_I2C_ADDR;

uint8_t accel_range = 0;
uint16_t gyro_range = 0;

static BMI08_INTF_RET_TYPE i2c_write_registers(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t *buf = (uint8_t *)malloc(len + 1);
    buf[0]       = reg_addr;
    memcpy(buf + 1, reg_data, len);
    uint8_t dev_addr = *(uint8_t *)intf_ptr;

    esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0, dev_addr, buf, len + 1, pdMS_TO_TICKS(1000));
    free(buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write error");
        return BMI08_E_COM_FAIL;
    }
    return BMI08_INTF_RET_SUCCESS;
}

static BMI08_INTF_RET_TYPE i2c_read_registers(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr = *(uint8_t *)intf_ptr;

    esp_err_t ret = i2c_master_write_read_device(I2C_NUM_0, dev_addr, &reg_addr, 1, reg_data, len, pdMS_TO_TICKS(1000));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write error");
        return BMI08_E_COM_FAIL;
    }
    return BMI08_INTF_RET_SUCCESS;
}

static void delay_us(uint32_t period, void *intf_ptr)
{
    ets_delay_us(period);
}

esp_err_t bean_imu_init()
{
    sensor          = (struct bmi08_dev *)malloc(sizeof(struct bmi08_dev));
    config_file_ptr = (uint8_t *)malloc(sizeof(uint8_t));
    accel_data      = (struct bmi08_sensor_data *)malloc(sizeof(struct bmi08_sensor_data));
    gyro_data       = (struct bmi08_sensor_data *)malloc(sizeof(struct bmi08_sensor_data));
    accel_data_f    = (struct bmi08_sensor_data_f *)malloc(sizeof(struct bmi08_sensor_data_f));
    gyro_data_f     = (struct bmi08_sensor_data_f *)malloc(sizeof(struct bmi08_sensor_data_f));

    sensor->intf_ptr_accel = &acc_dev_addr;
    sensor->intf_ptr_gyro  = &gyr_dev_addr;
    sensor->intf           = BMI08_I2C_INTF;
    sensor->variant        = BMI088_VARIANT;
    sensor->dummy_byte     = UINT8_C(0x00);
    sensor->accel_chip_id  = BMI088_ACCEL_CHIP_ID;
    sensor->intf_rslt      = BMI08_INTF_RET_SUCCESS;

    sensor->config_file_ptr = config_file_ptr;

    sensor->read_write_len = 46; //default 46

    sensor->read     = &i2c_read_registers;
    sensor->write    = &i2c_write_registers;
    sensor->delay_us = &delay_us;

    int8_t rslt = bmi088_mma_init(sensor); //accel init
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel init error");
        return ESP_FAIL;
    }

    rslt = bmi08g_init(sensor); //gyro init
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro init error");
        return ESP_FAIL;
    }
    for (int i = 0; i < 10; i++) {
        rslt = bmi08a_soft_reset(sensor);
        if (rslt == BMI08_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel soft reset error");
        return ESP_FAIL;
    }

    sensor->accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
    rslt                    = bmi08a_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set power mode error");
        return ESP_FAIL;
    }
    sensor->gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
    rslt                   = bmi08g_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set power mode error");
        return ESP_FAIL;
    }

    sensor->accel_cfg.odr   = BMI08_ACCEL_ODR_100_HZ;
    sensor->accel_cfg.range = BMI088_MM_ACCEL_RANGE_24G;
    accel_range             = 24;
    sensor->accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
    sensor->accel_cfg.bw    = BMI08_ACCEL_BW_NORMAL; /* Bandwidth and OSR are same */

    rslt = bmi08a_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set power mode error");
        return ESP_FAIL;
    }

    rslt = bmi088_mma_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set meas conf error");
        return ESP_FAIL;
    }

    sensor->gyro_cfg.odr   = BMI08_GYRO_BW_32_ODR_100_HZ;
    sensor->gyro_cfg.range = BMI08_GYRO_RANGE_2000_DPS;
    gyro_range             = 2000;
    sensor->gyro_cfg.bw    = BMI08_GYRO_BW_32_ODR_100_HZ;
    sensor->gyro_cfg.power = BMI08_GYRO_PM_NORMAL;

    rslt = bmi08g_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set power mode error");
        return ESP_FAIL;
    }

    rslt = bmi08g_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set power mode error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BMI088 init success!");
    return ESP_OK;
}

esp_err_t set_accel_odr(uint8_t odr)
{
    sensor->accel_cfg.odr = odr;
    int8_t rslt           = bmi088_mma_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set odr error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_accel_range(uint8_t range)
{
    sensor->accel_cfg.range = range;
    if (range == BMI088_ACCEL_RANGE_24G) {
        accel_range = 24;
    } else if (range == BMI088_ACCEL_RANGE_12G) {
        accel_range = 16;
    } else if (range == BMI088_ACCEL_RANGE_6G) {
        accel_range = 6;
    } else if (range == BMI088_ACCEL_RANGE_3G) {
        accel_range = 3;
    } else {
        ESP_LOGE(TAG, "BMI088 accel range not valid");
        return ESP_FAIL;
    }

    int8_t rslt = bmi088_mma_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set range error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_accel_bw(uint8_t bw)
{
    sensor->accel_cfg.bw = bw;
    int8_t rslt          = bmi088_mma_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set bw error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_accel_power_mode(uint8_t power_mode)
{
    sensor->accel_cfg.power = power_mode;
    int8_t rslt             = bmi08a_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel set power mode error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_gyro_odr(uint8_t odr)
{
    sensor->gyro_cfg.odr = odr;
    int8_t rslt          = bmi08g_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set odr error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_gyro_range(uint8_t range)
{
    sensor->gyro_cfg.range = range;
    if (range == BMI08_GYRO_RANGE_250_DPS) {
        gyro_range = 250;
    } else if (range == BMI08_GYRO_RANGE_500_DPS) {
        gyro_range = 500;
    } else if (range == BMI08_GYRO_RANGE_1000_DPS) {
        gyro_range = 1000;
    } else if (range == BMI08_GYRO_RANGE_2000_DPS) {
        gyro_range = 2000;
    } else {
        ESP_LOGE(TAG, "BMI088 gyro range not valid");
        return ESP_FAIL;
    }
    int8_t rslt = bmi08g_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set range error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_gyro_bw(uint8_t bw)
{
    sensor->gyro_cfg.bw = bw;
    int8_t rslt         = bmi08g_set_meas_conf(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set bw error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t set_gyro_power_mode(uint8_t power_mode)
{
    sensor->gyro_cfg.power = power_mode;
    int8_t rslt            = bmi08g_set_power_mode(sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro set power mode error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 * Function from the BMI08X Bosch Sensortec library
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 * Function from the BMI08X Bosch Sensortec library
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (dps / (half_scale)) * (val);
}

esp_err_t bean_imu_update_accel()
{
    //set_accel_power_mode(BMI08_ACCEL_PM_ACTIVE);
    if (accel_range == 0) {
        ESP_LOGE(TAG, "BMI088 accel range not set, cant measure yet");
        return ESP_FAIL;
    }

    int8_t rslt = bmi088_mma_get_data(accel_data, sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 accel update accel data error");
        return ESP_FAIL;
    }

    //printf("x = %d, y = %d, z = %d\n", accel_data->x, accel_data->y, accel_data->z);

    accel_data_f->x = lsb_to_mps2(accel_data->x, (float)accel_range, 16);
    accel_data_f->y = lsb_to_mps2(accel_data->y, (float)accel_range, 16);
    accel_data_f->z = lsb_to_mps2(accel_data->z, (float)accel_range, 16);

    return ESP_OK;
}

esp_err_t bean_imu_update_gyro()
{
    int8_t rslt = bmi08g_get_data(gyro_data, sensor);
    if (rslt != BMI08_OK) {
        ESP_LOGE(TAG, "BMI088 gyro get data error");
        return ESP_FAIL;
    }

    float x        = lsb_to_dps(gyro_data->x, (float)gyro_range, 16);
    float y        = lsb_to_dps(gyro_data->y, (float)gyro_range, 16);
    float z        = lsb_to_dps(gyro_data->z, (float)gyro_range, 16);
    gyro_data_f->x = x;
    gyro_data_f->y = y;
    gyro_data_f->z = z;

    return ESP_OK;
}

float get_x_accel_data()
{
    return accel_data_f->x;
}

float get_y_accel_data()
{
    return accel_data_f->y;
}

float get_z_accel_data()
{
    return accel_data_f->z;
}

float get_x_gyro_data()
{
    return gyro_data_f->x;
}

float get_y_gyro_data()
{
    return gyro_data_f->y;
}

float get_z_gyro_data()
{
    return gyro_data_f->z;
}
