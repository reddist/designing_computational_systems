//
// Created by bogdan on 20/09/2021.
//

#include "gpioutil.h"

void toggle(uint16_t pin) {
    HAL_GPIO_TogglePin(GPIOD, pin);
}

void wait (uint32_t time) {
    HAL_Delay(time);
}

void blink (uint16_t pin, uint32_t delay, uint16_t times) {
    for (uint16_t i = 0; i < times; i++) {
        toggle(pin);
        wait(delay);
        toggle(pin);
        wait(delay);
    }
}
