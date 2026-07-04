#pragma once
#include "esp_err.h"
#include "bean_context.h"

// Times are in ms, heights in m, accelerations in g

#define LAUNCH_ACCEL_THRESHOLD 12 //acceleration threshold to detect launch
#define LAUNCH_ACCEL_COUNT     5 //number of times acceleration threshold must be exceeded to detect launch

#define APOGEE_HEIGHT_THRESHOLD      2 //distance from apogee to deploy drogue
#define APOGEE_MIN_TIME_AFTER_LAUNCH 10200 //minimum time after launch to check for apogee in ms, safety feature
#define APOGEE_HEIGHT_COUNT          10 //number of times height must be below threshold to deploy drogue

#define PYRO_ACTIVE_TIME 1000 //time to keep pyro channel active in ms

#define MAIN_HEIGHT_DEPLOY     500 //distance above ground to deploy main
#define MAIN_TIME_AFTER_DROGUE 2000 //minimum time after drogue deployment before the altimeter failsafe may deploy main

#define LANDED_HEIGHT_THRESHOLD 100 //distance from ground to consider landed
#define LANDED_TIME_AFTER_MAIN  60000 //minimum time after main deployment to consider landed in ms

#define BARO_FAILSAFE_TIMEOUT 2000 //time without a successful altimeter read before deployment falls back to timers

#define DROGUE_CHUTE PIN_PYRO_0
#define MAIN_CHUTE   PIN_PYRO_1

typedef enum
{
    FLIGHT_STATE_PRELAUNCH = 0,
    FLIGHT_STATE_ARMED,
    FLIGHT_STATE_ASCENT,
    FLIGHT_STATE_DROGUE_OUT,
    FLIGHT_STATE_MAIN_OUT,
    FLIGHT_STATE_LANDED
} flight_state_t;

esp_err_t bean_flight_init(bean_context_t *ctx);
esp_err_t bean_flight_goto_state(flight_state_t new_state);
flight_state_t bean_flight_get_state(void);
const char *bean_flight_state_name(flight_state_t state);
