#pragma once

#include "FspTimer.h"
#include "pwm_zen.h" //customized PWM library
#include "BD79702.h" //DAC

class HW_adsr {
public:
    HW_adsr(uint8_t atk_pin, uint8_t dec_pin, uint8_t rel_pin, Dac& dac);

    //PWM stages are first, then sustain
    enum Stage { ATTACK, DECAY, RELEASE, SUSTAIN, STAGE_COUNT};

    void begin();
    void setRate(Stage stage, float rate);
    void setSustain(float sustain);

    float values[STAGE_COUNT];
private:
    struct stage {
        float maxVal; //scale "CV"
        float offset; //offset

        float curve;
        float reciprocal;

        float rate;
        //uint8_t pin;
        PWM* timer;
    };

    Dac& _dac; //pointer to external DAC object
    stage _stages[3]; //hold all PWM stages
};