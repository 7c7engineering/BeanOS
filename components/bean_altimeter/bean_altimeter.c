#include <stdio.h>
#include "bean_altimeter.h"

bool _filterEnabled, _tempOSEnabled, _presOSEnabled, _ODREnabled;
uint8_t _i2caddr;
int32_t _sensorID;
int8_t _cs;

double bmp390_pressure = 0;
double bmp390_temperature = 0;

int8_t bmp390_address = 0x76;
struct bmp3_dev * sensor;
struct bmp3_settings * settings;

static const char *TAG = "BMP390";

// Our hardware interface functions

static int8_t i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t *buf = (uint8_t *)malloc(len + 1);
    buf[0] = reg_addr;
    memcpy(buf + 1, reg_data, len);
    esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0, bmp390_address, buf, len + 1, pdMS_TO_TICKS(1000));
    free(buf);
    if(ret == ESP_OK){
        return BMP3_OK;
    }
    ESP_LOGE(TAG, "I2C write failed with error %d", ret);
    return BMP3_E_COMM_FAIL;
}

static int8_t i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    esp_err_t ret = i2c_master_write_read_device(I2C_NUM_0, bmp390_address, &reg_addr, 1, reg_data, len, pdMS_TO_TICKS(1000));
    if(ret == ESP_OK){
        return BMP3_OK;
    }
    ESP_LOGE(TAG, "I2C read failed with error %d", ret);
    return BMP3_E_COMM_FAIL;
}

static void delay_usec(uint32_t us, void *intf_ptr)
{
    ets_delay_us(us);
}

static int8_t cal_crc(uint8_t seed, uint8_t data)
{
    int8_t poly = 0x1D;
    int8_t var2;
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if ((seed & 0x80) ^ (data & 0x80))
        {
            var2 = 1;
        }
        else
        {
            var2 = 0;
        }

        seed = (seed & 0x7F) << 1;
        data = (data & 0x7F) << 1;
        seed = seed ^ (uint8_t)(poly * var2);
    }

    return (int8_t)seed;
}

static int8_t validate_trimming_param(struct bmp3_dev *dev)
{
    int8_t rslt;
    uint8_t crc = 0xFF;
    uint8_t stored_crc;
    uint8_t trim_param[21];
    uint8_t i;

    rslt = bmp3_get_regs(BMP3_REG_CALIB_DATA, trim_param, 21, dev);
    if (rslt == BMP3_OK)
    {
        for (i = 0; i < 21; i++)
        {
            crc = (uint8_t)cal_crc(crc, trim_param[i]);
        }

        crc = (crc ^ 0xFF);
        rslt = bmp3_get_regs(0x30, &stored_crc, 1, dev);
        if (stored_crc != crc)
        {
            rslt = -1;
        }
    }

    return rslt;
}

esp_err_t bean_altimeter_init()
{
    sensor = (struct bmp3_dev *) malloc(sizeof(struct bmp3_dev));
    settings = (struct bmp3_settings *) malloc(sizeof(struct bmp3_settings));
    sensor->chip_id = bmp390_address;
    sensor->intf = BMP3_I2C_INTF;
    sensor->read = &i2c_read;
    sensor->write = &i2c_write;
    sensor->delay_us = &delay_usec;
    sensor->intf_ptr = &i2c_write;
    sensor->dummy_byte = 0x00;

    int8_t rslt = BMP3_OK;

    rslt = bmp3_init(sensor);
    if (rslt != BMP3_OK)
    {
        ESP_LOGE(TAG, "BMP3 initialization failed with code %hhi", rslt);
        return ESP_FAIL;
    }

    rslt = validate_trimming_param(sensor);
    if (rslt != BMP3_OK)
    {
        ESP_LOGE(TAG, "BMP3 trimming parameters validation failed");
        return ESP_FAIL;
    }

    setTemperatureOversampling(BMP3_NO_OVERSAMPLING);
    setPressureOversampling(BMP3_NO_OVERSAMPLING);
    setIIRFilterCoeff(BMP3_IIR_FILTER_DISABLE);
    setOutputDataRate(BMP3_ODR_25_HZ);

    // don't do anything till we request a reading
    settings->op_mode = BMP3_MODE_FORCED;

    return ESP_OK;
}

esp_err_t bean_altimeter_update()
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel = 0;
    /* Variable used to select the sensor component */
    uint8_t sensor_comp = 0;

    /* Select the pressure and temperature sensor to be enabled */
    settings->temp_en = BMP3_ENABLE;
    settings_sel |= BMP3_SEL_TEMP_EN;
    sensor_comp |= BMP3_TEMP;
    if (_tempOSEnabled)
    {
        settings_sel |= BMP3_SEL_TEMP_OS;
    }

    settings->press_en = BMP3_ENABLE;
    settings_sel |= BMP3_SEL_PRESS_EN;
    sensor_comp |= BMP3_PRESS;
    if (_presOSEnabled)
    {
        settings_sel |= BMP3_SEL_PRESS_OS;
    }

    if (_filterEnabled)
    {
        settings_sel |= BMP3_SEL_IIR_FILTER;
    }

    if (_ODREnabled)
    {
        settings_sel |= BMP3_SEL_ODR;
    }

    rslt = bmp3_set_sensor_settings(settings_sel, settings, sensor);
    if (rslt != BMP3_OK)
    {
        ESP_LOGE(TAG, "BMP3 set sensor settings failed");
        return ESP_FAIL;
    }
    settings->op_mode = BMP3_MODE_FORCED;
    rslt = bmp3_set_op_mode(settings, sensor);
    if (rslt != BMP3_OK)
    {
        ESP_LOGE(TAG, "BMP3 set operation mode failed");
        return ESP_FAIL;
    }
    struct bmp3_data data;
    rslt = bmp3_get_sensor_data(sensor_comp, &data, sensor);
    if (rslt != BMP3_OK)
    {
        ESP_LOGE(TAG, "BMP3 get sensor data failed");
        return ESP_FAIL;
    }

    bmp390_pressure = data.pressure;
    bmp390_temperature = data.temperature;
    return ESP_OK;
}

double bean_altimeter_get_temperature()
{
    return bmp390_temperature;
}

double bean_altimeter_get_pressure()
{
    return bmp390_pressure;
}

esp_err_t setTemperatureOversampling(uint8_t oversample)
{
    if (oversample > BMP3_OVERSAMPLING_32X)
    {
        return ESP_FAIL;
    }
    settings->odr_filter.temp_os = oversample;
    if (oversample == BMP3_NO_OVERSAMPLING)
    {
        _tempOSEnabled = false;
    }
    else
    {
        _tempOSEnabled = true;
    }
    return ESP_OK;
}

esp_err_t setPressureOversampling(uint8_t oversample)
{
    if (oversample > BMP3_OVERSAMPLING_32X)
    {
        return ESP_FAIL;
    }
    settings->odr_filter.press_os = oversample;
    if (oversample == BMP3_NO_OVERSAMPLING)
    {
        _presOSEnabled = false;
    }
    else
    {
        _presOSEnabled = true;
    }
    return ESP_OK;
}

esp_err_t setIIRFilterCoeff(uint8_t coef)
{
    if (coef > BMP3_IIR_FILTER_COEFF_7)
    {
        return ESP_FAIL;
    }
    settings->odr_filter.iir_filter = coef;
    if (coef == BMP3_IIR_FILTER_DISABLE)
    {
        _filterEnabled = false;
    }
    else
    {
        _filterEnabled = true;
    }
    return ESP_OK;
}

esp_err_t setOutputDataRate(uint8_t odr)
{
    if (odr > BMP3_ODR_0_001_HZ)
    {
        return ESP_FAIL;
    }

    settings->odr_filter.odr = odr;
    _ODREnabled = true;
    return ESP_OK;
}