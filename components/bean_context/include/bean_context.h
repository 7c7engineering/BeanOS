#pragma once

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "pins.h"
#include <stdbool.h>
#include "cJSON.h"

typedef struct bean_context
{
    EventGroupHandle_t system_event_group;
    QueueHandle_t event_queue;
    QueueHandle_t data_log_queue;
    bool is_not_usb_msc;

    // Configuration fields
    struct
    {
        bool data_logging_enabled;
        bool event_logging_enabled;
    } config;
} bean_context_t;

typedef enum measurement_type
{
    MEASUREMENT_TYPE_TEMPERATURE,
    MEASUREMENT_TYPE_PRESSURE,
    MEASUREMENT_TYPE_ALTITUDE,
    MEASUREMENT_TYPE_ACCELERATION,
    MEASUREMENT_TYPE_GYROSCOPE,
    MEASUREMENT_TYPE_BATTERY_VOLTAGE
} measurement_type_t;

typedef struct event_data
{
    int event_id;
    uint32_t timestamp;
    char *event_data;
} event_data_t;

typedef struct log_data
{
    measurement_type_t measurement_type;
    uint32_t timestamp;
    char measurement_value[20];
} log_data_t;

typedef struct config_merge_result
{
    bool config_changed;
    bool needs_storage_update;
    int items_updated;
    int items_ignored;
    int type_mismatches;
} config_merge_result_t;

const cJSON *config_store_get(void);

config_merge_result_t bean_context_initialize_config(cJSON *config);

esp_err_t bean_context_init(bean_context_t **ctx);
