#include "systemio.h"
#include "bean_context.h"
#include "esp_check.h"
static char tag[] = "systemio";
static i2c_master_bus_handle_t s_i2c_bus;

esp_err_t io_init()
{
    if (s_i2c_bus != NULL)
    {
        return ESP_OK;
    }

    i2c_master_bus_config_t conf = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&conf, &s_i2c_bus), tag, "Failed to initialize I2C master bus");
    return ESP_OK;
}
