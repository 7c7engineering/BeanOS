#pragma once
#include "esp_err.h"
#include "bean_context.h"

esp_err_t bean_battery_init(bean_context_t *ctx);
bool bean_battery_is_usb_powered(void);
int bean_battery_get_voltage_mv(void); // -1 until the first measurement completes
