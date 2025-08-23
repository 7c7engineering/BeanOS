#include <stdio.h>
#include "bean_beep.h"
#include "driver/mcpwm.h"
#include "bean_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"




#define BEEP_MCPWM_UNIT      MCPWM_UNIT_0
#define BEEP_MCPWM_TIMER     MCPWM_TIMER_0

esp_err_t bean_beep_init()
{
	// Initialize MCPWM GPIOs
	mcpwm_gpio_init(BEEP_MCPWM_UNIT, MCPWM0A, PIN_BUZ_P);
	mcpwm_gpio_init(BEEP_MCPWM_UNIT, MCPWM0B, PIN_BUZ_N);

	// Configure MCPWM timer for default 2kHz, 50% duty
	mcpwm_config_t pwm_config = {
		.frequency = 2000,
		.cmpr_a = 50.0,
		.cmpr_b = 50.0,
		.duty_mode = MCPWM_DUTY_MODE_0,
		.counter_mode = MCPWM_UP_COUNTER,
	};
	esp_err_t err = mcpwm_init(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, &pwm_config);
	if (err != ESP_OK) return err;

	// Set complementary output: A = PWM, B = inverted PWM
	mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
	mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, MCPWM_DUTY_MODE_1); // Inverted
	return ESP_OK;
}

esp_err_t bean_beep_sound(uint32_t frequency, uint32_t duration)
{
	// Set frequency and 50% duty
	mcpwm_set_frequency(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, frequency); // Not available in ESP-IDF, so:
	mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, 50.0);
	mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, 50.0);
	mcpwm_set_duty_type(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, MCPWM_DUTY_MODE_1); // Inverted

	// Wait for duration
	vTaskDelay(duration / portTICK_PERIOD_MS);

	// Set duty to 0 to silence
	mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_A, 0.0);
	mcpwm_set_duty(BEEP_MCPWM_UNIT, BEEP_MCPWM_TIMER, MCPWM_OPR_B, 0.0);
	return ESP_OK;
}
