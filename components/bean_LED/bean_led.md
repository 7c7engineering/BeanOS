# Bean LED component

A component that provides control over the LED indicators on the device.

## Hardware
There are 2 RGB leds on the board called L1 and L2. These LEDs can be controlled individually to display different states of the system.

## Implementation
The LEDs are controlled by the LEDC peripheral and PWM-ed at 1 kHz. (for now). The resolution of the LEDC peripheral is set to 13 bits. This means we can take 8-bit values and map them to the 13-bit range through a gamma lookup table. This will ensure good color rendering and low-brightness performance.

## Usage
To use the LED component, include the header file and initialize the component.

Each RGB color is bundled into a `led_color_rgb_t` structure.

```c
#include "bean_led.h"

void app_main(void)
{
    bean_led_init();
    bean_led_set_color(LED_L1, (led_color_rgb_t){255, 0, 0}); // Set L1 to red
    bean_led_set_color(LED_L2, (led_color_rgb_t){0, 255, 0}); // Set L2 to green
    bean_led_set_color(LED_BOTH, (led_color_rgb_t){0, 0, 255}); // Set both LEDs to blue
}
```

## TODO's
 - 📖 Documentation about generating the gamma lookup table using python.
 - Make some pre-defined color structures.
 - Have a LED status update freeRTOS task:
    - Load the LED status settings file from the SPIFFS.
    - Look at the device status through the context pointer.
    - Animate the LEDs based on the device status.
