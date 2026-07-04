#pragma once
#include "esp_err.h"
#include "cJSON.h"

// Brings up SoftAP (always) and STA (if bean_webui.wifi.sta is enabled with a
// non-empty SSID), then announces mdns_hostname.local. Returns ESP_OK as long
// as the AP starts; STA join failures only log.
esp_err_t bean_webui_wifi_init(const cJSON *webui_cfg);
esp_err_t bean_webui_wifi_stop(void);
