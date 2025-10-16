#pragma once
#include "freertos/event_groups.h"
#include "esp_system.h"

const static EventBits_t BEAN_SYSTEM_INIT_OK =
  BIT0; // Indicates the system is fully initialized, this bit can be used to synchronize the start of other tasks
const static EventBits_t BEAN_SYSTEM_ERROR       = BIT1; // Indicates the system is in an error state
const static EventBits_t BEAN_SYSTEM_BATTERY_LOW = BIT2; // Indicates the battery is low, warn user
const static EventBits_t BEAN_SYSTEM_BATTERY_CRITICAL =
  BIT3; // Indicates the battery very low, device should be turned off
const static EventBits_t BEAN_SYSTEM_BATTERY_CHARGING = BIT4; // Indicates the battery is charging
const static EventBits_t BEAN_SYSTEM_BATTERY_FULL     = BIT5; // Indicates the battery is full
const static EventBits_t BEAN_SYSTEM_USB_POWERED      = BIT6; // Indicates the system is powered via USB