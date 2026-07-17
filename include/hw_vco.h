#pragma once

#include <Arduino.h>
#include "modulator.h" //Software modulators
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC
#include "parameter.h"

class HW_VCO {
public:
    enum Waveform : uint8_t { SAW, PULSE, TRIANGLE, SUB_SAW, SUB_PULSE, SUB_TRI,
        PULSE_SAW, PULSE_INV_SAW, PULSE_TRI,
        SUB_PULSE_SAW, SUB_PULSE_INV_SAW, SUB_PULSE_TRI,
        SUPER_SAW, SUPER_INV_SAW, DOUBLE_PULSE, SUPER_TRI,
        SUB_SUP_SAW, SUB_SUP_INV_SAW, SUB_DUB_PULSE, SUB_SUP_TRI, NO_WAVE, WAVE_COUNT };

    SW_LFO pwmLFO{ "PWM LFO" };
    SW_DA pwmDA{ "PWM LFO" }; //delay attack envelope
    SW_ADSR pwmADSR{ "PWM Env" };

    Modulator* _modulators[3];

    bool legato = true;

    //osc limits----------------------------------------------------
    static constexpr uint8_t LOW_NOTE = 24; //C1
    static constexpr uint8_t HIGH_NOTE = 108; //C8 7 octaves

    //MIDI note
    static constexpr uint8_t C5 = 60, C2 = 36, NOTE_A4 = 69;

    HW_VCO(DigiPot& pwmPot);
    void setNoteHW(float note);
    void calibrate();
    float measureFreq();
    float getAvgFreq();
    void update(); //steps through all modulators and update outputs
    void gateOn(bool gate);
    void gateOff();
    void setPWMhw(float newValue);
    void setWaveformHW(Waveform waveform);
    Modulator * findOwner(Param* param);
    static void initTimer(); //static method to set up timer once

    Param waveform { "Waveform", _waveform.getStr };
    Param pwm { "PWM" };
    Param pitch { "Pitch" }; //ignore CC. Pass value through .modulation
    float note;
private:
    //assign bit positions to the components of a waveform
    enum waveformBits { saw_b = 1, sqr1_b = (1 << 1), sqr2_b = (1 << 2), tri_b =(1 << 3), sub_b = (1 << 4), inv_saw_b = (1 << 5) };
    enum waveformFlags { saw = 1, sqr1 = (1 << 1), sqr2 = (1 << 2), tri = saw | (1 << 3), sub = (1 << 4), inv_saw = saw | (1 << 5) };

    static constexpr const char* WaveformNames[WAVE_COUNT] = {
            [SAW]               = "Saw",
            [PULSE]             = "Pulse",
            [TRIANGLE]          = "Triangle",

            [SUB_SAW]           = "Sub + Saw",
            [SUB_PULSE]         = "Sub + Pulse",
            [SUB_TRI]           = "Sub + Tri",

            [PULSE_SAW]         = "Pulse + Saw",
            [PULSE_INV_SAW]     = "Pulse + Inv Saw",
            [PULSE_TRI]         = "Pulse + Tri",

            [SUB_PULSE_SAW]     = "Sub + Pulse + Saw",
            [SUB_PULSE_INV_SAW] = "Sub + Pulse + Inv Saw",
            [SUB_PULSE_TRI]     = "Sub + Pulse + Tri",

            [SUPER_SAW]         = "Super Saw",
            [SUPER_INV_SAW]     = "Inv Super Saw",
            [DOUBLE_PULSE]      = "Double Pulse",
            [SUPER_TRI]         = "Super Tri",

            [SUB_SUP_SAW]       = "Sub + Sup Saw",
            [SUB_SUP_INV_SAW]   = "Sub + Inv Sup Saw",
            [SUB_DUB_PULSE]     = "Sub + Dub Pulse",
            [SUB_SUP_TRI]       = "Sub + Sup Tri",

            [NO_WAVE]           = "No Wave"
    };

    //make helper struct for Enum Params. Makes string function, converts/stores enum
    EnumParam<Waveform, WAVE_COUNT, WaveformNames, 21> _waveform;

    //settings that define each waveform
    static constexpr uint8_t waveformDefinitions[WAVE_COUNT] = {
            [SAW]               = saw,
            [PULSE]             = sqr1,
            [TRIANGLE]          = tri,

            [SUB_SAW]           = sub | saw,
            [SUB_PULSE]         = sub | sqr1,
            [SUB_TRI]           = sub | tri,

            [PULSE_SAW]         = saw | sqr1,
            [PULSE_INV_SAW]     = inv_saw | sqr1,
            [PULSE_TRI]         = tri | sqr1,

            [SUB_PULSE_SAW]     = sub | saw | sqr1,
            [SUB_PULSE_INV_SAW] = sub | inv_saw | sqr1,
            [SUB_PULSE_TRI]     = sub | tri | sqr1,

            [SUPER_SAW]         = saw | sqr1 | sqr2,
            [SUPER_INV_SAW]     = inv_saw | sqr1 | sqr2,
            [DOUBLE_PULSE]      = sqr1 | sqr2,
            [SUPER_TRI]         = tri | sqr1 | sqr2,

            [SUB_SUP_SAW]       = sub | saw | sqr1 | sqr2,
            [SUB_SUP_INV_SAW]   = sub | inv_saw | sqr1 | sqr2,
            [SUB_DUB_PULSE]     = sub | sqr1 | sqr2,
            [SUB_SUP_TRI]       = sub | tri | sqr1 | sqr2,

            [NO_WAVE]           = 0
    };

    //Waveform _waveform;
    #define SAW_SW A1 //pins to control waveform
    #define SUB_SW A2
    #define PUL1_SW A3
    #define PUL2_SW A4
    #define TRI_SW A5
    //D7 pin that measures Osc frequency
    #define DAC_STEPS 4095.0 //number of steps in 12-bit DAC
    #define NOTE_RANGE 96 //guess at max note range for DAC

    //tuning values-------------------------------------------------
    static constexpr uint16_t A4tuning = 440;
    float tuneScaling;
    float tuningOffset;

    //timer values--------------------------------------------------
    //Asynchronous General Purpose Timer interrupt (from vector_data.h)
    static const IRQn_Type IRQn_CCMPA = AGT0_INT_IRQn;

    static constexpr uint8_t READINGS_MAX = 16; //number of timing readings to take
    inline static volatile uint32_t lastDelta = 0;
    inline static volatile uint32_t readingsBuffer[READINGS_MAX] = {}; //store multiple input capture readings
    inline static volatile uint8_t readingsIndex = 0;
    inline static volatile bool readingsComplete = false; //flag to show buffer has been filled

    static void captureISR(); //interrupt service routine

    DigiPot& _pwmPot;
};