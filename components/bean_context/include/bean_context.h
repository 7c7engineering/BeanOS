#pragma once

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "bean_bits.h"
#include "pins.h"



typedef struct bean_context
{
    EventGroupHandle_t system_event_group;
    QueueHandle_t event_queue;
    QueueHandle_t data_log_queue;
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
    int32_t measurement_value;
} log_data_t;

esp_err_t bean_context_init(bean_context_t **ctx);
