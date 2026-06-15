#pragma once

#include "parameter.h"
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC

class HW_LFO {
public:
    enum Waveform { TRIANGLE, SQUARE, WAVE_COUNT };

    HW_LFO(Dac& rateDac) : _rateDac(rateDac) {}
    Param waveform { "Waveform" };
    Param rate { "LFO Rate" };

    void update();
    //TODO: figure out approximate LFO rates
    static const char* getRateStr(char* buf, size_t len, uint8_t v);
    static const char* getWaveformStr(char* buffer, size_t size, uint8_t value);
private:
    static constexpr const char* WaveformNames[] = { [TRIANGLE] = "Triangle", [SQUARE] = "Square" };

    static constexpr float MIN_HZ = 0.01f;
    static constexpr float MAX_HZ = 400.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    Dac& _rateDac;
};