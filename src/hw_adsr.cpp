#include "hw_adsr.h"

void HW_adsr::begin() {
    //_dac->begin();
    setSustain(.5); //default

    for (Stage stage: { ATTACK, DECAY, RELEASE }) {
        auto& [maxVal, offset, curve, reciprocal, rate, timer] = _stages[stage];
        reciprocal = 1 / (curve - 1.0); //precalc reciprocal
        timer->begin(10000); //start timer
        setRate(stage, rate); //set init rate
    }
}

void HW_adsr::setRate(Stage stage, float newRate) {
    if (stage == SUSTAIN) return setSustain(newRate);

    //get members of specific stage
    auto& [maxVal, offset, curve, reciprocal, rate, timer] = _stages[stage];

    values[stage] = newRate;

    float expoVal = 1; //default to fastest
    if (newRate != 0) {
        newRate = 1 - ((newRate + offset) * maxVal); //apply scale and offset
        expoVal = (pow(curve, newRate) - 1) * reciprocal; //exponentiate
    }

    rate = newRate;
    timer->setDutyCycle(expoVal);
}

void HW_adsr::setSustain(float sustain) {
    values[SUSTAIN] = sustain;

    //only use half the range because env maxes out at 2.5V
    //TODO: fix this in hardware

    _dac.write( sustain * 0.5);
}

HW_adsr::HW_adsr(uint8_t atk_pin, uint8_t dec_pin, uint8_t rel_pin, Dac& dac) : _dac(dac){
    //set up the stages. Note: pins used must support PWM
    _stages[ATTACK] = { 58/127.0, 18/127.0, 500000.0, 0, .1, new PWM(atk_pin) };
    _stages[DECAY] = { 103/127.0, 0, 50000.0, 0, .5, new PWM(dec_pin) };
    _stages[RELEASE] = { 103/127.0, 0, 50000.0, 0, .5, new PWM(rel_pin) };
}
