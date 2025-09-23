#pragma once
#include "esp_err.h"

esp_err_t bean_beep_init();
esp_err_t bean_beep_sound(uint32_t frequency, uint32_t duration_ms);

enum notes
{
    NOTE_C4 = 261,
    NOTE_D4 = 294,
    NOTE_E4 = 329,
    NOTE_F4 = 349,
    NOTE_G4 = 392,
    NOTE_A4 = 440,
    NOTE_B4 = 493,
    NOTE_C5 = 523,
    NOTE_D5 = 587,
    NOTE_E5 = 659,
    NOTE_F5 = 698,
    NOTE_G5 = 784,
    NOTE_A5 = 880,
    NOTE_B5 = 988,
    NOTE_C6 = 1047,
    NOTE_D6 = 1175,
    NOTE_E6 = 1319,
    NOTE_F6 = 1397,
    NOTE_G6 = 1568,
    NOTE_A6 = 1760,
    NOTE_B6 = 1976,
    NOTE_C7 = 2093,
    NOTE_D7 = 2349,
    NOTE_E7 = 2637,
    NOTE_F7 = 2794,
    NOTE_G7 = 3136,
    NOTE_A7 = 3520,
    NOTE_B7 = 3951,
    NOTE_C8 = 4186,
    NOTE_D8 = 4699,
    NOTE_E8 = 5274,
    NOTE_F8 = 5588,
    NOTE_G8 = 6272
};
