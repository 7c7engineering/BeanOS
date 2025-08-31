# Bean Pyro Component

A component that provides control over the 4 pyro-channels on the device for rocket deployment and other pyrotechnic functions.

## Hardware
There are 4 pyro channels on the board:
- **PYRO_0**: GPIO 38
- **PYRO_1**: GPIO 37  
- **PYRO_2**: GPIO 14
- **PYRO_3**: GPIO 15

Each channel can be independently controlled to fire pyrotechnic devices for a specified duration.

## Features
- **Individual Channel Control**: Fire each pyro channel independently
- **Duration Control**: Specify how long each channel remains active
- **Status Tracking**: Keep track of which channels have been fired and when
- **Safety Features**: Prevent re-firing of already used channels
- **Bulk Operations**: Fire all channels simultaneously if needed
- **Reset Capability**: Reset channels for testing purposes

## Usage

### Initialization
```c
#include "bean_pyro.h"

void app_main(void) {
    // Initialize the pyro component
    esp_err_t ret = bean_pyro_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pyro component");
        return;
    }
}
```

### Firing Individual Channels
```c
// Fire PYRO_0 for 100ms
esp_err_t ret = bean_pyro_fire(PYRO_0, 100);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to fire PYRO_0");
}

// Fire PYRO_1 for 200ms
ret = bean_pyro_fire(PYRO_1, 200);
```

### Firing All Channels
```c
// Fire all pyro channels for 150ms
ret = bean_pyro_fire_all(150);
```

### Checking Channel Status
```c
// Check if a specific channel has been fired
if (bean_pyro_is_fired(PYRO_0)) {
    ESP_LOGI(TAG, "PYRO_0 has been fired");
}

// Get detailed status of a channel
pyro_channel_status_t status;
ret = bean_pyro_get_channel_status(PYRO_1, &status);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "PYRO_1 fired: %s, duration: %d ms", 
              status.fired ? "yes" : "no", status.duration);
}

// Get status of all channels
pyro_channel_status_t all_status[4];
ret = bean_pyro_get_all_status(all_status);
```

### Getting Fired Count
```c
// Count how many channels have been fired
uint8_t fired_count = bean_pyro_get_fired_count();
ESP_LOGI(TAG, "%d pyro channels have been fired", fired_count);
```

### Resetting Channels (for testing)
```c
// Reset a specific channel
ret = bean_pyro_reset_channel(PYRO_2);

// Reset all channels
ret = bean_pyro_reset_all();
```

## Error Handling
The component returns `ESP_ERR_INVALID_STATE` if attempting to fire an already-fired channel. Always check return values and handle errors appropriately in safety-critical applications.
