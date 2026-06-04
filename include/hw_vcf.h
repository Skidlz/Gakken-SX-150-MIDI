#pragma once

#include "Arduino.h"
#include "BD79702.h" //DAC
#include "MCP4251.h" //DigiPot

class HW_VCF {
public:
    enum Mode { LO_PASS, HI_PASS };

    Mode mode = LO_PASS;
    float cut;
    float resonance;
    float keyTracking = .3;

    HW_VCF(Dac& dac, DigiPot& pot) : _cutDac(dac), _resonancePot(pot) {}
    void updateCut(float currentNote, float offset);
    void updateResonance(float newValue);
    void setMode(Mode newMode);
private:
    Dac& _cutDac;
    DigiPot& _resonancePot;
};