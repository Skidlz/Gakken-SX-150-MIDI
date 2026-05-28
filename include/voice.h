#pragma once

#include <Arduino.h>
#include "oscillator.h"

//used to track things like pitch bend, portamento, current note, etc

class Voice {
public:
    Oscillator osc;

    bool glideOn;
    //bool glideLegato;
    float glideBlend; //blend between fixed-rate and fixed-time portamento

    int8_t bendRange;
    float currentBend; //current bend amount
    float modDepth; //vibrato depth

    void setGlideRate(float rate);
    void updateGlide(); //call periodically to update glide progress
    void noteOn(uint8_t note, uint8_t vel);
    void noteOff(uint8_t note, uint8_t vel);
    void setPitchBend(int16_t bend);
    //void setModDepth(float depth);
    void setParams(uint8_t cc, uint8_t val);
    Voice();
private:
    float currentNote; //current note without any glide, modulation, bend
    float currentGlideNote; //current note without any modulation,bend
    float targetNote;

    float glideRate;
    float glideStep; //how much to step by to keep glide time fixed
    float glideAlpha; //used for RC curve
};