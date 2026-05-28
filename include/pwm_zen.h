//PWM library that supports 2 output per channel
#pragma once

#include "FspTimer.h"
#include <Arduino.h>

class PWM {
public:
    uint16_t period; //period in counts

    PWM(uint8_t pin) : _pin(pin) { }
    void begin(uint32_t freq);
    void setDutyCycle(float duty);
private:
    static FspTimer* _timers[8]; //hold all timers so they can be shared between instances

    FspTimer* _timer; //GPT#
    uint8_t _pin; //output pin
    TimerPWMChannel_t _pwmChan; //A/B
};