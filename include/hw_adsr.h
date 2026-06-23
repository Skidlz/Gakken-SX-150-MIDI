#pragma once

#include "FspTimer.h"
#include "pwm_zen.h" //customized PWM library
#include "parameter.h"
#include "BD79702.h" //DAC

class HW_ADSR {
public:
    HW_ADSR(uint8_t atk_pin, uint8_t dec_pin, uint8_t rel_pin, Dac& dac);
    //PWM stages are first, then sustain
    enum Stage { ATTACK, DECAY, RELEASE, SUSTAIN, STAGE_COUNT };
    enum Polarity { NORMAL, INVERT, POL_COUNT };

    void begin();
    void setRate(Stage stage, float rate);
    void setSustain(float sustain);
    void setPolarityHW(Polarity newPolarity);
    void gateOn(bool gate);
    void gateOff();
    void update();

    bool legato = true;

    Param attack { "Attack" };
    Param decay { "Decay" };
    Param sustain { "Sustain" };
    Param release { "Release" };
    Param polarity { "Polarity", _polarity.getStr };
private:
    struct stage {
        float maxVal; //scale CC
        float offset;
        float curve;
        float reciprocal;
        PWM* timer;
    };

    //make helper struct for Enum Params. Makes string function, converts/stores enum
    static constexpr const char* PolarityModes[] = { [NORMAL] = "Normal", [INVERT] = "Invert" };
    EnumParam<Polarity, POL_COUNT, PolarityModes, 7> _polarity;

    #define GATE_PIN D6 //ADSR Gate Pin
    //TODO: move this pin to IO expander
    #define INV_PIN A7 //pin that inverts the ADSR

    Dac& _dac; //pointer to external DAC object
    stage _stages[3]; //hold all PWM stages
};