#pragma once

#include <Arduino.h>
#include "hw_vco.h"
#include "hw_vcf.h"
#include "hw_lfo.h"
#include "hw_adsr.h"
#include "modulator.h" //Software modulators
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC
#include "parameter.h"

//used to track things like pitch bend, portamento, current note, etc

class Voice {
public:
    Oscillator osc;
    HW_VCF vcf;
    HW_LFO lfo;
    HW_adsr& env;
    SW_ADSR accentADSR { "Acc Env" };
    SW_ADSR vcaADSR { "VCA Env" };

    bool gate = false;
    bool glideOn = false;
    bool glideLegato = true;
    float glideBlend; //blend between fixed-rate and fixed-time portamento

    int8_t bendRange = 2; //±2 semitone
    float currentBend = 0; //current bend amount
    uint8_t accentThreshold = 100; //velocity must be over this to trigger accent envelope
    float modDepth = 0; //vibrato depth

    float currentNote = Oscillator::NOTE_A4; //current note without any glide, modulation, bend
    float currentGlideNote = Oscillator::NOTE_A4; //current note without any modulation,bend

    Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& vcfDrivePot, DigiPot& pwmPot, Dac& lfoRateDac, HW_adsr& adsr);
    void setGlideTime(float time);
    void updateGlide(); //call at TICK_RATE to update glide progress
    void updateTargetNote(uint8_t note);
    void noteOn(uint8_t note, uint8_t vel);
    void noteOff(uint8_t note, uint8_t vel);
    void setPitchBend(int16_t bend);
    void update(); //steps through all modulators and update outputs

    Param glideTime { "Glide Time", getGlideTime };
    Param vcfAccAmt { "VCF Accent" };
private:
    float _targetNote;

    float _glideStep; //how much to step by to keep glide time fixed
    float _glideAlpha; //used for RC curve
    static constexpr float MIN_GLIDE_TM = 0.12; //120ms

    static constexpr float TICK_RATE = 4000.0f;

    static const char* getGlideTime(char* buffer, size_t size, uint8_t value);
};