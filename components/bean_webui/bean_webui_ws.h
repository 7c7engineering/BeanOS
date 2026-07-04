#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t bean_webui_ws_register(httpd_handle_t server);
bool bean_webui_ws_has_clients(void);
void bean_webui_ws_broadcast(const char *json, size_t len);
