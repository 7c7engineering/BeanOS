#include <stdio.h>
#include "bean_context.h"

esp_err_t bean_context_init(bean_context_t **ctx)
{

    *ctx = (bean_context_t *)malloc(sizeof(bean_context_t));
    if (!*ctx) return ESP_ERR_NO_MEM;

    (*ctx)->system_event_group = xEventGroupCreate();
    if (!(*ctx)->system_event_group) return ESP_ERR_NO_MEM;

    (*ctx)->event_queue = xQueueCreate(10, sizeof(event_data_t));
    if (!(*ctx)->event_queue) return ESP_ERR_NO_MEM;

    (*ctx)->data_log_queue = xQueueCreate(10, sizeof(log_data_t));
    if (!(*ctx)->data_log_queue) return ESP_ERR_NO_MEM;

    return ESP_OK;
}