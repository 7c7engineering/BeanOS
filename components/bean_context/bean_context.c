#include "bean_context.h"

esp_err_t bean_context_init(bean_context_t **ctx)
{

    *ctx = (bean_context_t *)malloc(sizeof(bean_context_t));

    (*ctx)->is_not_usb_msc = false;

    // Set default config, will be overwritten or used by bean_storage
    (*ctx)->config.data_logging_enabled  = true;
    (*ctx)->config.event_logging_enabled = true;

    if (!*ctx)
        return ESP_ERR_NO_MEM;

    (*ctx)->system_event_group = xEventGroupCreate();
    if (!(*ctx)->system_event_group)
        return ESP_ERR_NO_MEM;

    (*ctx)->event_queue = xQueueCreate(10, sizeof(event_data_t));
    if (!(*ctx)->event_queue)
        return ESP_ERR_NO_MEM;

    (*ctx)->data_log_queue = xQueueCreate(10, sizeof(log_data_t));
    if (!(*ctx)->data_log_queue)
        return ESP_ERR_NO_MEM;

    return ESP_OK;
}
