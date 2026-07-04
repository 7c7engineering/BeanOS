#pragma once
#include "esp_err.h"
#include "bean_context.h"

esp_err_t bean_webui_sampler_start(bean_context_t *ctx, int update_hz);
void bean_webui_sampler_stop(void);
