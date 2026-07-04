#pragma once
#include "esp_err.h"
#include "bean_context.h"

// Starts WiFi (SoftAP + optional STA), mDNS, the HTTP server and the sensor
// sampler task. No-op returning ESP_OK when config bean_webui.enabled is false.
esp_err_t bean_webui_init(bean_context_t *ctx);

// Hook for the future flight-state machine: when config
// bean_webui.wifi.disable_on_takeoff is true this stops WiFi and the sampler.
// Nothing calls this yet.
void bean_webui_notify_takeoff(void);
