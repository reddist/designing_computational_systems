//
// Created by bogdan on 20/09/2021.
//

#ifndef ENGINEERING_COMPUTATIONAL_SYSTEMS_GPIOUTIL_H
#define ENGINEERING_COMPUTATIONAL_SYSTEMS_GPIOUTIL_H

#define PIN_RED GPIO_PIN_15
#define PIN_YELLOW GPIO_PIN_14
#define PIN_GREEN GPIO_PIN_13

void toggle(uint16_t pin);
void blink (uint16_t pin, uint32_t delay, uint16_t times);
void wait (uint32_t time);

#endif //ENGINEERING_COMPUTATIONAL_SYSTEMS_GPIOUTIL_H
