#pragma once

#include "Arduino.h"
#include "parameter.h"
#include "modulator.h"
#include "BD79702.h" //DAC
#include "MCP4251.h" //DigiPot

class HW_VCF {
public:
    enum Mode { LO_PASS, HI_PASS, MODE_COUNT };

    HW_VCF(Dac& cutoffDac, DigiPot& resonancePot, DigiPot& drivePot) :
        _cutoffDac(cutoffDac),
        _resonancePot(resonancePot),
        _drivePot(drivePot) {}
    void updateCutHW(float newCut);
    void update();
    void setModeHW(Mode newMode);
    ValueSource keyTrackMod; //dummy Modulator to pass keytracking

    Param cutoff { "Cutoff" };
    Param resonance { "Resonance" };
    Param keyTracking { "Key Track" };
    Param drive { "VCF Drive" };
    Param mode { "VCF Mode", _mode.getStr };
private:
    static constexpr const char* modeNames[] = { [LO_PASS] = "Low Pass", [HI_PASS] = "High Pass" };
    EnumParam<Mode, MODE_COUNT, modeNames, 10> _mode;

    Dac& _cutoffDac;
    DigiPot& _resonancePot;
    DigiPot& _drivePot;
};