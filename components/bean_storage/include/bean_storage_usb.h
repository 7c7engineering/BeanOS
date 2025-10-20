#pragma once
#include "esp_err.h"
#include "wear_levelling.h"
#include <stdbool.h>

#define STORAGE_BASE_PATH "/extflash"
esp_err_t bean_storage_usb_init(wl_handle_t s_wl_handle);
esp_err_t storage_expose_usb(void);
esp_err_t storage_hide_usb(void);
bool storage_is_exposed_usb(void);
