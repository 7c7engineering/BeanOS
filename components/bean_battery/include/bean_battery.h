#pragma once
#include "esp_err.h"
#include "bean_context.h"

esp_err_t bean_battery_init(bean_context_t *ctx);
bool bean_battery_is_usb_powered(void);
