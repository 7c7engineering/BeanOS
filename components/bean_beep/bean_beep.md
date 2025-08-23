# Bean Beep component

A component that is responsible for generating sound on the piezo buzzer. Beep, boop, beep!
## Hardware
The hardware is connected to 2 GPIOs on the ESP32. These GPIO's can then be driven in anti-phase to create a differential signal across the piezo buzzer at double the I/O voltage.

## Implementation
The MCPWM peripheral is used to generate the PWM signals for the piezo buzzer with variable frequency at 50% duty cycle.

## Usage
To use the bean_beep component, include the header file, init the component and call the appropriate functions to generate sound.

Predefined notes are also available in an enum after including the header file.

Example:

```c
#include "bean_beep.h"

void app_main() {
    bean_beep_init();
    bean_beep_sound(NOTE_C6, 1000);  // Play C5 note for 1000ms
}
```

## TODO's
 - Add support for half-volume using 1 PWM channel and 1 fixed GPIO.
 - Make the `bean_beep_sound()` function non-blocking during the tone.
 - Do some basic input parameter validation.