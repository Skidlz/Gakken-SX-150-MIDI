#pragma once

#include "Arduino.h"
#include "parameter.h"
#include "BD79702.h" //DAC
#include "MCP4251.h" //DigiPot

class HW_VCF {
public:
    enum Mode { LO_PASS, HI_PASS };

    Mode mode = LO_PASS;

    HW_VCF(Dac& cutoffDac, DigiPot& resonancePot, DigiPot& drivePot) :
        _cutoffDac(cutoffDac),
        _resonancePot(resonancePot),
        _drivePot(drivePot) {}
    void updateCut(float currentNote, float offset);
    void update(float currentGlideNote, float offset);
    void setMode(Mode newMode);

    Param cutoff { "Cutoff" };
    Param resonance { "Resonance" };
    Param keyTracking { "Key Track" };
    Param drive { "Drive" };
private:
    Dac& _cutoffDac;
    DigiPot& _resonancePot;
    DigiPot& _drivePot;
};