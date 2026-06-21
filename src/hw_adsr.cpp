#include "hw_adsr.h"

HW_adsr::HW_adsr(uint8_t atk_pin, uint8_t dec_pin, uint8_t rel_pin, Dac& dac) : _dac(dac){
    //set up the stages. Note: pins used must support PWM
    _stages[ATTACK] = { 58/127.0, 18/127.0, 500000.0, 0, new PWM(atk_pin) };
    _stages[DECAY] = { 103/127.0, 0, 50000.0, 0, new PWM(dec_pin) };
    _stages[RELEASE] = { 103/127.0, 0, 50000.0, 0, new PWM(rel_pin) };
}

void HW_adsr::begin() {
    pinMode(GATE_PIN, OUTPUT);
    //_dac->begin();
    setSustain(.5); //default

    float initRates[] = { [ATTACK] = .1, [DECAY] = .5, [RELEASE]= .5 };

    for (Stage stage: { ATTACK, DECAY, RELEASE }) {
        auto& [maxVal, offset, curve, reciprocal, timer] = _stages[stage];
        reciprocal = 1 / (curve - 1.0); //precalc reciprocal
        timer->begin(10000); //start timer
        setRate(stage, initRates[stage]); //set init rate
    }
}

void HW_adsr::setRate(Stage stage, float newRate) {
    if (stage == SUSTAIN) return setSustain(newRate);

    //get members of specific stage
    auto& [maxVal, offset, curve, reciprocal, timer] = _stages[stage];

    float expoVal = 1; //default to fastest
    if (newRate != 0) {
        newRate = 1 - ((newRate + offset) * maxVal); //apply scale and offset
        expoVal = (pow(curve, newRate) - 1) * reciprocal; //exponentiate
    }

    timer->setDutyCycle(expoVal);
}

void HW_adsr::setSustain(float sustain) {
    //only use half the range because env maxes out at 2.5V
    //TODO: fix this in hardware

    _dac.write( sustain * 0.5);
}

void HW_adsr::setPolarityHW(Polarity newPolarity) {
    digitalWrite(INV_PIN, (newPolarity == INVERT));
}

void HW_adsr::update() {
    if (attack.dirty) setRate(ATTACK, attack.get());
    if (decay.dirty) setRate(DECAY, decay.get());
    if (sustain.dirty) setSustain(sustain.get());
    if (release.dirty) setRate(RELEASE, release.get());

    if (_polarity.update(polarity)) setPolarityHW(_polarity.value);
}

void HW_adsr::gateOn(bool gate) {
    if (gate) {
        if (legato) return; //don't retrigger

        digitalWrite(GATE_PIN, LOW); //force retrigger
        delayMicroseconds(10);
    }

    digitalWrite(GATE_PIN, HIGH);
}

void HW_adsr::gateOff() {
    digitalWrite(GATE_PIN, LOW);
}