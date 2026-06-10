#pragma once

#include <Arduino.h>
#include "modulator.h" //Software modulators
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC
#include "parameter.h"

class Oscillator {
public:
    enum Waveform { SAW, PULSE, TRIANGLE, SUB_SAW, SUB_PULSE, SUB_TRI,
        PULSE_SAW, PULSE_INV_SAW, PULSE_TRI,
        SUB_PULSE_SAW, SUB_PULSE_INV_SAW, SUB_PULSE_TRI,
        SUPER_SAW, SUPER_INV_SAW, DOUBLE_PULSE, SUPER_TRI,
        SUB_SUP_SAW, SUB_SUP_INV_SAW, SUB_DUB_PULSE, SUB_SUP_TRI, NO_WAVE, WAVE_COUNT };

    float currentNote;
    bool running;

    Oscillator(DigiPot& pwmPot);

    void setNote(float note);
    void calibrate();
    float measureFreq();
    float getAvgFreq();
    void start();
    void stop();
    void updatePWM(float offset);
    void setWaveform(Waveform waveform);

    static constexpr uint8_t READINGS_MAX = 16; //number of timing readings to take

    static void initTimer(); //static method to set up timer once

    //osc limits----------------------------------------------------
    static constexpr uint8_t LOW_NOTE = 24; //C1
    static constexpr uint8_t HIGH_NOTE = 108; //C8 7 octaves

    //tuning values-------------------------------------------------
    float tuneScaling;
    float tuningOffset;

    //MIDI note
    static constexpr uint8_t C5 = 60;
    static constexpr uint8_t NOTE_A4 = 69;
    static constexpr uint8_t C2 = 36;

    //PWM modulation------------------------------------------------
    float pulseWidth = .5;
    SW_LFO pwmLFO {"PWM LFO"};
    SW_DA pwmDA; //delay attack envelope
    SW_ADSR pwmADSR;

    Param waveform { "Waveform", getWaveformStr };
private:
    //assign bit positions to the components of a waveform
    enum waveformBits { saw_b = 1, sqr1_b = (1 << 1), sqr2_b = (1 << 2), tri_b =(1 << 3), sub_b = (1 << 4), inv_saw_b = (1 << 5) };
    enum waveformFlags { saw = 1, sqr1 = (1 << 1), sqr2 = (1 << 2), tri = saw | (1 << 3), sub = (1 << 4), inv_saw = saw | (1 << 5) };

    struct WaveformDefinition {
        uint8_t config;
        const char* name; //string to show on OLED
    };

    //settings that define each waveform
    static constexpr WaveformDefinition WAVEFORM_TABLE[WAVE_COUNT] = {
        [SAW]               = { .config = saw,                          .name = "Saw" },
        [PULSE]             = { .config = sqr1,                         .name = "Pulse" },
        [TRIANGLE]          = { .config = tri,                          .name = "Triangle" },

        [SUB_SAW]           = { .config = sub | saw,                    .name = "Sub + Saw" },
        [SUB_PULSE]         = { .config = sub | sqr1,                   .name = "Sub + Pulse" },
        [SUB_TRI]           = { .config = sub | tri,                    .name = "Sub + Tri" },

        [PULSE_SAW]         = { .config = saw | sqr1,                   .name = "Pulse + Saw" },
        [PULSE_INV_SAW]     = { .config = inv_saw | sqr1,               .name = "Pulse + Inv Saw" },
        [PULSE_TRI]         = { .config = tri | sqr1,                   .name = "Pulse + Tri" },

        [SUB_PULSE_SAW]     = { .config = sub | saw | sqr1,             .name = "Sub + Pulse + Saw" },
        [SUB_PULSE_INV_SAW] = { .config = sub | inv_saw | sqr1,         .name = "Sub + Pulse + Inv Saw" },
        [SUB_PULSE_TRI]     = { .config = sub | tri | sqr1,             .name = "Sub + Pulse + Tri" },

        [SUPER_SAW]         = { .config = saw | sqr1 | sqr2,            .name = "Super Saw" },
        [SUPER_INV_SAW]     = { .config = inv_saw | sqr1 | sqr2,        .name = "Inv Super Saw" },
        [DOUBLE_PULSE]      = { .config = sqr1 | sqr2,                  .name = "Double Pulse" },
        [SUPER_TRI]       = { .config = tri | sqr1 | sqr2,              .name = "Super Tri" },

        [SUB_SUP_SAW]       = { .config = sub | saw | sqr1 | sqr2,      .name = "Sub + Sup Saw" },
        [SUB_SUP_INV_SAW]   = { .config = sub | inv_saw | sqr1 | sqr2,  .name = "Sub + Inv Sup Saw" },
        [SUB_DUB_PULSE]     = { .config = sub | sqr1 | sqr2,            .name = "Sub + Dub Pulse" },
        [SUB_SUP_TRI]       = { .config = sub | tri | sqr1 | sqr2,      .name = "Sub + Sup Tri" },

        [NO_WAVE]           = { .config = 0,                            .name = "No Wave" }
    };

    Waveform _waveform;
    #define SAW_SW A1 //pins to control waveform
    #define SUB_SW A2
    #define PUL1_SW A3
    #define PUL2_SW A4
    #define TRI_SW A5
    #define GATE_PIN D6 //pin that turns the osc on and off
    //D7 pin that measures Osc frequency
    #define DAC_STEPS 4095.0 //number of steps in 12-bit DAC
    #define NOTE_RANGE 96 //guess at max note range for DAC

    static constexpr uint16_t A4tuning = 440;

    //timer values--------------------------------------------------
    //Asynchronous General Purpose Timer interrupt (from vector_data.h)
    static const IRQn_Type IRQn_CCMPA = AGT0_INT_IRQn;

    inline static volatile uint32_t lastDelta = 0;
    inline static volatile uint32_t readingsBuffer[READINGS_MAX] = {}; //store multiple input capture readings
    inline static volatile uint8_t readingsIndex = 0;
    inline static volatile bool readingsComplete = false; //flag to show buffer has been filled

    static void captureISR(); //interrupt service routine

    static const char* getWaveformStr(char* buffer, size_t size, uint8_t value);

    DigiPot& _pwmPot;
};