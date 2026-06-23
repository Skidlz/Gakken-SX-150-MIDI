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
#include "route.h"

class Voice {
public:
    HW_VCO osc;
    HW_VCF vcf;
    HW_LFO lfo;
    HW_ADSR& env;
    SW_ADSR accentADSR { "Acc Env" };
    SW_ADSR vcaADSR { "VCA Env" };

    bool gate = false;
    bool glideOn = false;
    bool glideLegato = true;

    int8_t bendRange = 2; //±2 semitone
    uint8_t accentThreshold = 100; //velocity must be over this to trigger accent envelope

    //Route handling variables
    static constexpr uint8_t NUM_ROUTES = 20;
    Param* destinations[NUM_ROUTES];
    size_t destinationCount = 0;
    Route routes[NUM_ROUTES] = {
//        Route(&osc.pwmLFO, &osc.pitch, &accentADSR, "Route 1"),
//        Route(&accentADSR, &routes[0].depth, nullptr, "Route 2"),
        Route(&osc.pwmLFO, nullptr, &accentADSR, "Route 1"),
        Route(&routes[0], &osc.pitch, nullptr, "Route 2"),
    };

    Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& vcfDrivePot, DigiPot& pwmPot, Dac& lfoRateDac, HW_ADSR& adsr);
    void setGlideTime(float time);
    void updateGlide(); //call at TICK_RATE to update glide progress
    void updateTargetNote(uint8_t note);
    void noteOn(uint8_t note, uint8_t vel);
    void noteOff(uint8_t note, uint8_t vel);
    void setPitchBend(int16_t bend);
    void update(); //steps through all modulators and update outputs
    void buildDestinations();

    Param glideTime { "Glide Time", getGlideTime };
    Param vcfAccAmt { "VCF Accent" };
private:
    float currentBend = 0; //current bend amount
    float currentGlideNote = HW_VCO::NOTE_A4; //current note without any modulation,bend
    float _targetNote;

    float _glideAlpha; //used for RC curve
    static constexpr float MIN_GLIDE_TM = 0.12; //120ms

    static constexpr float TICK_RATE = 4000.0f;

    static const char* getGlideTime(char* buffer, size_t size, uint8_t value);
};