#include <stdio.h>
#pragma once

class touchStrip {
public:
    touchStrip();
    void begin();
    void poll();
    bool gate = false;
    float output = 0.0;
    void (*pressedCB)(float reading);
    void (*updatedCB)(float reading);
    void (*releasedCB)(float reading);

private:
    static constexpr int16_t MAX_14_BIT = 16383; //2^14
    static constexpr int16_t LOW_THRESH = MAX_14_BIT * .31; //strip only mid 1/3 of 0-5v
    static constexpr int16_t MAX_READING = MAX_14_BIT * .33; //strip only mid 1/3 of 0-5v

    static constexpr uint16_t POLL_RATE = 4000; //4kHz
    static constexpr uint16_t MIN_GATE_TIME = 50; //50ms
    static constexpr uint16_t MIN_GATE_TICKS = (POLL_RATE * MIN_GATE_TIME) / 1000; //50ms

    static constexpr uint16_t SMOOTH_MAX_DELTA = 250; //only smooth jumps smaller than this

    uint16_t gateTimer = 0; //enforce a minimum gate time to avoid bounce

    //reading skews while releasing, so buffer old values
    static constexpr uint8_t HIST_COUNT = 16; //must be pow of 2
    float history[HIST_COUNT] = { 0 }; //4ms buffer @ 4kHz
    uint8_t writeIndex = 0;

    int16_t prevReading = 0;

    float smoothedReading = 0; //remove jitter
    static constexpr float SMOOTH_ALPHA = .02;

    uint16_t adc_cfg = 0;

    //normalize reading as a float
    static inline float scaleAndOffset(int16_t reading) { return (float) (reading - LOW_THRESH) / MAX_READING; }
    //get next index in circular history buffer
    static inline uint8_t nextIndex(int16_t index) { return (index + 1) & (HIST_COUNT - 1); }
};