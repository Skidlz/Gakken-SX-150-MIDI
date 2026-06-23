#pragma once
#include "Arduino.h"
#include "parameter.h"

#define CAST_MOD(cls, member) reinterpret_cast<Modulator cls::*>(&cls::member)

class Modulator {
public:
    virtual void step() = 0;
    virtual void gateOn() {};
    virtual void gateOff() {};
    float output = 0;
    float scale = 1;

private:

protected: //consts for child classes
    static constexpr float TICK_RATE = 4000.0f;
    static constexpr float TICK_RATE_INV = 1/TICK_RATE;
};

//LFO--------------------------------------------------------------------------
class SW_LFO : public Modulator {
public:
    enum Waveform : uint8_t { TRIANGLE, SQUARE, SINE, SAW, NOISE, WAVE_COUNT };

    SW_LFO(const char* p = "LFO"); //default prefix is LFO
    void step() override;
    void gateOn() override;
    void setRate(float rate);
    void setSlew(float rate);

    Param rate;
    Param depth;
    Param waveform;
    Param reset;
    Param slew;
private:
    static constexpr const char* WaveformNames[] = {
        [TRIANGLE]  = "Triangle",
        [SQUARE]    = "Square",
        [SINE]      = "Sine",
        [SAW]       = "Saw",
        [NOISE]     = "Noise"
    };

    //make helper struct for Enum Params. Makes string function, converts/stores enum
    EnumParam<Waveform, WAVE_COUNT, WaveformNames, 9> _waveform;

    static constexpr uint32_t MAX = UINT32_MAX; //max count
    static constexpr uint32_t HALF_MAX = MAX / 2;
    static constexpr float MIN_HZ = 0.01f;
    static constexpr float MAX_HZ = 100.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    static constexpr float MIN_SLEW = 0.01f;
    static constexpr float MAX_SLEW = 100.0f;
    static constexpr float SLEW_RANGE = MAX_SLEW / MIN_SLEW; //precalc slew range

    const char* prefix;
    static const char* getRateStr(char* buffer, size_t size, uint8_t value);
    static const char* getSlewStr(char* buffer, size_t size, uint8_t value);
    static const char* getPhaseStr(char* buffer, size_t size, uint8_t value);

    float _rate = .01;
    uint32_t _stepSize = 1;
    uint32_t _phase = 0;
    float _slewRate = 0;
    float _previousValue = 0;
    float _currentSlewed = 0;
};

//Clock-------------------------------------------------------------------------
class SW_CLOCK : public Modulator {
public:
    SW_CLOCK(const char* p);
    void step() override;
    void gateOn() override;
    void setRate(float rate);
    bool output = false;
    bool sync = true;

    Param rate;
private:
    static constexpr uint32_t MAX = UINT32_MAX; //max count
    static constexpr uint32_t HALF_MAX = MAX / 2;
    static constexpr float MIN_HZ = 4.0f;
    static constexpr float MAX_HZ = 200.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    const char* prefix;
    static const char* getRateStr(char* buf, size_t len, uint8_t v);

    float _rate = 1;
    uint32_t _stepSize = 1;
    uint32_t _phase = 0;
};

//ADSR-------------------------------------------------------------------------
class SW_ADSR : public Modulator {
public:
    //PWM stages are first, then sustain
    enum Stage { ATTACK, DECAY, RELEASE, SUSTAIN, STAGE_COUNT};
    //enum Mode { ADSR, ASR, AD };
    SW_ADSR(const char* p);
    void step() override;
    void setRate(Stage stage, float newRate);
    void setSustain(float sustain);

    void gateOn() override;
    void gateOff() override;

    SW_CLOCK sampHoldClock { "ADSR" }; //TODO add prefix support

    float values[STAGE_COUNT];

    Param attack;
    Param decay;
    Param sustain;
    Param release;
    //Param depth;
private:
    struct stage {
        float minPeriod;
        float maxPeriod;
        float range;

        float alpha; //used in RC curve math
        float rate;
        float target; //value to move towards
    };

    const char* prefix;

    static constexpr float MIN_RATE = .0001;

    stage _stages[3]; //hold all PWM stages
    Stage _currentStage;

    float _phase = 0;
};

//Delay Attack-----------------------------------------------------------------
class SW_DA : public Modulator {
public:
    //PWM stages are first, then sustain
    enum Stage { DELAY, ATTACK, STALL };
    SW_DA(const char* p);
    void step() override;
    void setRate(Stage stage, float newRate);

    void gateOn() override;
    void gateOff() override;

    float values[2];

    Param delay;
    Param attack;
private:
    struct stage {
        float minPeriod;
        float maxPeriod;
        float range;

        float alpha; //step size
        float rate;
    };

    const char* prefix;

    static constexpr float MIN_RATE = .0001;

    stage _stages[3]; //hold all PWM stages
    Stage _currentStage;

    float _phase = 0;
};