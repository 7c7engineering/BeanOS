#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Pyro channel enumeration
typedef enum {
    PYRO_0 = 0,
    PYRO_1 = 1,
    PYRO_2 = 2,
    PYRO_3 = 3,
    PYRO_ALL = 4  // Special case for firing all channels
} pyro_channel_t;

// Pyro channel status structure
typedef struct {
    bool fired;
    uint32_t fire_time;  // Timestamp when fired (in ms since boot)
    uint32_t duration;   // Duration the channel was fired (in ms)
} pyro_channel_status_t;

// Initialize the pyro component and configure GPIO pins
esp_err_t bean_pyro_init(void);

// Fire a specific pyro channel for a given duration
esp_err_t bean_pyro_fire(pyro_channel_t channel, uint32_t duration_ms);

// Fire all pyro channels for a given duration
esp_err_t bean_pyro_fire_all(uint32_t duration_ms);

// Check if a specific channel has been fired
bool bean_pyro_is_fired(pyro_channel_t channel);

// Get the status of a specific channel
esp_err_t bean_pyro_get_channel_status(pyro_channel_t channel, pyro_channel_status_t *status);

// Get the status of all channels
esp_err_t bean_pyro_get_all_status(pyro_channel_status_t *status);

// Reset the fired status of a specific channel (for testing purposes)
esp_err_t bean_pyro_reset_channel(pyro_channel_t channel);

// Reset the fired status of all channels (for testing purposes)
esp_err_t bean_pyro_reset_all(void);

// Get the number of channels that have been fired
uint8_t bean_pyro_get_fired_count(void);
