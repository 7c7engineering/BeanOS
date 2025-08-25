# Bean Beep component

A component that is responsible for generating sound on the piezo buzzer. Beep, boop, beep!

## Hardware
The hardware is connected to 2 GPIOs on the ESP32. These GPIO's can then be driven in anti-phase to create a differential signal across the piezo buzzer at double the I/O voltage.

## Implementation
The MCPWM peripheral is used to generate the PWM signals for the piezo buzzer with variable frequency at 50% duty cycle.

**NEW**: The beep function is now non-blocking using FreeRTOS timers for automatic shutdown.

## Usage
To use the bean_beep component, include the header file, init the component and call the appropriate functions to generate sound.

Predefined notes are also available in an enum after including the header file.

Example:

```c
#include "bean_beep.h"

void app_main() {
    bean_beep_init();
    
    // This now returns immediately - beep plays in background
    bean_beep_sound(NOTE_C6, 1000);  // Play C6 note for 1000ms
    
    // Your code continues here without waiting
    ESP_LOGI(TAG, "This message appears immediately");
    
    // Check if beep is still playing
    if (bean_beep_is_playing()) {
        ESP_LOGI(TAG, "Beep is still playing in background");
    }
}
```

## API Functions

- `bean_beep_init()` - Initialize the component
- `bean_beep_sound(frequency, duration_ms)` - Play beep (non-blocking)
- `bean_beep_stop()` - Stop current beep
- `bean_beep_is_playing()` - Check if beep is active

## Non-blocking Operation

The `bean_beep_sound()` function now:
- Returns immediately without blocking
- Uses a FreeRTOS timer to automatically stop the beep after the specified duration
- Allows your code to continue executing while the beep plays in the background

## TODO's
 - Add support for half-volume using 1 PWM channel and 1 fixed GPIO.
 - Do some basic input parameter validation.