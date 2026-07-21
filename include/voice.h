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
#include <vector>

class Voice {
public:
    HW_VCO osc;
    HW_VCF vcf;
    HW_LFO lfo;
    HW_ADSR& env;
    SW_ADSR accentADSR { "Acc Env" };
    SW_ADSR vcaADSR { "VCA Env" };
    SW_LFO lfo1 { "LFO 1"};
    SW_LFO lfo2 { "LFO 2"};
    SH_Slew samphold1 { "S/H 1"};

    bool gate = false;
    bool glideOn = false;
    bool glideLegato = true;

    int8_t bendRange = 2; //±2 semitone
    uint8_t accentThreshold = 100; //velocity must be over this to trigger accent envelope

    //Route handling variables
    static constexpr uint8_t NUM_ROUTES = 20;
    std::vector<Modulator *> modSort;
    uint8_t routesUsed = 6;

    Route routes[NUM_ROUTES] = {
        Route(&osc.pwmLFO, &osc.pwm, &osc.pwmDA, "PWM LFO"), //DA envelope fades in PWM lfo
        Route(&osc.pwmADSR, &osc.pwm, nullptr, "PWM ADSR"),
        Route(&accentADSR, &vcf.cutoff, nullptr, "Act Env > Cut"),
        Route(&vcf.keyTrackMod, &vcf.cutoff, nullptr, "Keytracking"),
        //Route(&lfo1, &lfo2.rate, nullptr, "LFO1 > LFO2"),
        //Route(&lfo2, &vcf.cutoff, nullptr, "LFO2 > VCF"),
        Route(&lfo1, &samphold1.input, nullptr, "LFO1 > S/H"),
        Route(&samphold1, &vcf.cutoff, nullptr, "S/H > VCF"),
    };

    //TODO: move static routes to end of array
    //aliases
    Route& pwmLFO = routes[0];
    Route& pwmADSR = routes[1];
    Route& vcfAccAmt = routes[2];
    Route& keyTrack = routes[3];

    Route& lfoSH = routes[4];

    Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& vcfDrivePot, DigiPot& pwmPot, Dac& lfoRateDac, HW_ADSR& adsr);
    void setGlideTime(float time);
    void updateGlide(); //call at TICK_RATE to update glide progress
    void updateTargetNote(uint8_t note);
    void noteOn(uint8_t note, uint8_t vel);
    void noteOff(uint8_t note, uint8_t vel);
    void setPitchBend(int16_t bend);
    void update(); //steps through all modulators and update outputs
    void evalRoutes();
    Modulator* findOwner(Param* param);

    SimpleParam glideTime { "Glide Time", getGlideTime };
private:
    float currentBend = 0; //current bend amount
    float currentGlideNote = HW_VCO::NOTE_A4; //current note without any modulation,bend
    float _targetNote;

    float _glideAlpha; //used for RC curve
    static constexpr float MIN_GLIDE_TM = 0.12; //120ms

    static constexpr float TICK_RATE = 4000.0f;

    static const char* getGlideTime(char* buffer, size_t size, uint8_t value);
};