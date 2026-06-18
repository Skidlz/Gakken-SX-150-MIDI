#pragma once

#include "FspTimer.h"
#include "pwm_zen.h" //customized PWM library
#include "parameter.h"
#include "BD79702.h" //DAC

class HW_adsr {
public:
    HW_adsr(uint8_t atk_pin, uint8_t dec_pin, uint8_t rel_pin, Dac& dac);

    //PWM stages are first, then sustain
    enum Stage { ATTACK, DECAY, RELEASE, SUSTAIN, STAGE_COUNT};

    void begin();
    void setRate(Stage stage, float rate);
    void setSustain(float sustain);
    void gateOn(bool gate);
    void gateOff();
    void update();

    bool legato = true;

    Param attack { "Attack" };
    Param decay { "Decay" };
    Param sustain { "Sustain" };
    Param release { "Release" };
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

    #define GATE_PIN D6 //pin that turns the osc on and off

    Dac& _dac; //pointer to external DAC object
    stage _stages[3]; //hold all PWM stages
};