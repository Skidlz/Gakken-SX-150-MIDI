#pragma once

#include <Arduino.h>
#include "modulator.h" //Software modulators
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC

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
    SW_LFO pwmLFO;
    SW_DA pwmDA; //delay attack envelope
    SW_ADSR pwmADSR;
private:
    //assign bit positions to the components of a waveform
    enum waveformBits { saw_b = 1, pulse1_b = (1 << 1), pulse2_b = (1 << 2), tri_b =(1 << 3), sub_b = (1 << 4), inv_saw_b = (1 << 5) };
    enum waveformFlags { saw = 1, pulse1 = (1 << 1), pulse2 = (1 << 2), tri = saw | (1 << 3), sub = (1 << 4), inv_saw = saw | (1 << 5) };
    //settings that define each waveform
    static constexpr uint8_t waveformConfig[WAVE_COUNT] = {
            [SAW] = saw,
            [PULSE] = pulse1,
            [TRIANGLE] = tri,

            [SUB_SAW] = sub | saw, //octave saw
            [SUB_PULSE] = sub | pulse1,
            [SUB_TRI] = sub | tri,

            [PULSE_SAW] = saw | pulse1,
            [PULSE_INV_SAW] = inv_saw | pulse1,
            [PULSE_TRI] = tri | pulse1,
            [SUB_PULSE_SAW] = sub | saw | pulse1,
            [SUB_PULSE_INV_SAW] = sub | inv_saw | pulse1,
            [SUB_PULSE_TRI] = sub |  tri | pulse1,

            [SUPER_SAW] = saw | pulse1 | pulse2,
            [SUPER_INV_SAW] = inv_saw | pulse1 | pulse2,
            [DOUBLE_PULSE] = pulse1 | pulse2,
            [SUPER_TRI] = tri | pulse1 | pulse2,
            [SUB_SUP_SAW] = sub | saw | pulse1 | pulse2,
            [SUB_SUP_INV_SAW] = sub | inv_saw | pulse1 | pulse2,
            [SUB_DUB_PULSE] = sub | pulse1 | pulse2, //step saw
            [SUB_SUP_TRI] = sub | tri | pulse1 | pulse2,

            [NO_WAVE] = 0
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

    DigiPot& _pwmPot;
};