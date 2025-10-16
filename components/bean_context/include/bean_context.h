#pragma once

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "bean_bits.h"
#include "pins.h"
#include "portmacro.h"
#include <stdbool.h>

typedef struct bean_context
{
    EventGroupHandle_t system_event_group;
    QueueHandle_t event_queue;
    QueueHandle_t data_log_queue;
    bool is_not_usb_msc;
    bool is_metrcis_enabled;
    TickType_t last_height_requested_at;
    float max_height;
} bean_context_t;

typedef enum measurement_type
{
    MEASUREMENT_TYPE_TEMPERATURE,
    MEASUREMENT_TYPE_PRESSURE,
    MEASUREMENT_TYPE_ALTITUDE,
    MEASUREMENT_TYPE_ACCELERATION_X,
    MEASUREMENT_TYPE_ACCELERATION_Y,
    MEASUREMENT_TYPE_ACCELERATION_Z,
    MEASUREMENT_TYPE_GYROSCOPE,
    MEASUREMENT_TYPE_BATTERY_VOLTAGE,
    MEASUREMENT_TYPE_STAGE_TRANSITION
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

esp_err_t bean_context_init(bean_context_t **ctx);
