#pragma once

#include "parameter.h"
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC

class HW_LFO {
public:
    HW_LFO(Dac& rateDac);
    enum Waveform { TRIANGLE, SQUARE, WAVE_COUNT };
    enum Reset { FREE_RUN, SYNC, LEGATO, RESET_COUNT };

    void gateOn(bool gate);
    void gateOff();
    void update();
    void setWaveformHW(Waveform waveform);
    //TODO: figure out approximate LFO rates
    static const char* getRateStr(char* buffer, size_t size, uint8_t value);

    Param waveform { "LFO Waveform", _waveform.getStr };
    Param rate { "LFO Rate", getRateStr };
    Param resetMode { "LFO Reset", _resetMode.getStr };
private:
    #define RESET_PIN D2

    static constexpr const char* WaveformNames[] = { [TRIANGLE] = "Triangle", [SQUARE] = "Square" };
    static constexpr const char* ResetModes[] = { [FREE_RUN] = "Free Running", [SYNC] = "Sync", [LEGATO] = "Legato" };

    EnumParam<Waveform, WAVE_COUNT, WaveformNames, 9> _waveform;
    EnumParam<Reset, RESET_COUNT, ResetModes, 13> _resetMode;

    static constexpr float MIN_HZ = 0.01f;
    static constexpr float MAX_HZ = 400.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    Dac& _rateDac;
};