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
    void setResetMode(Reset newMode);
    void setWaveform(Waveform waveform);
    //TODO: figure out approximate LFO rates
    static const char* getRateStr(char* buffer, size_t size, uint8_t value);
    static const char* getWaveformStr(char* buffer, size_t size, uint8_t value);
    static const char* getResetStr(char* buffer, size_t size, uint8_t value);

    Param waveform { "LFO Waveform", getWaveformStr };
    Param rate { "LFO Rate", getRateStr };
    Param reset { "LFO Reset", getResetStr };
private:
    Reset _resetMode = FREE_RUN;
    #define RESET_PIN D2

    static constexpr float WAVE_SCALE = 127.0 * WAVE_COUNT / 128.0;
    static constexpr float RESET_SCALE = 127.0 * RESET_COUNT / 128.0;
    static constexpr const char* WaveformNames[] = { [TRIANGLE] = "Triangle", [SQUARE] = "Square" };
    static constexpr const char* ResetModes[] = { [FREE_RUN] = "Free Running", [SYNC] = "Sync", [LEGATO] = "Legato" };

    static constexpr float MIN_HZ = 0.01f;
    static constexpr float MAX_HZ = 400.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    Dac& _rateDac;
};