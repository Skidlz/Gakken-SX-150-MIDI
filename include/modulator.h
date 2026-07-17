#pragma once
#include "Arduino.h"
#include "parameter.h"

class Modulator {
public:
    virtual void step() = 0;
    virtual void gateOn() {};
    virtual void gateOff() {};
    float output = 0;
    float scale = 1;

    Modulator* findOwner(const Param* param) { //return 'this' if we own a given Param
        //tests if a param is within our memory range, meaning we own it
        return (param >= (const void*)this && param < (const void*)(this + 1)) ? this : nullptr;
    }
protected: //consts for child classes
    static constexpr float TICK_RATE = 4000.0f;
    static constexpr float TICK_RATE_INV = 1/TICK_RATE;
};

class ValueSource : public Modulator {
public:
    float input = 0.0f; //write arbitrary value here to use as a modulator output

    void step() override {
        output = input;
    }
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
    Param waveform;
    Param reset;
    Param slew;
private:
    //list of Params that we manage on step()
    static constexpr Param SW_LFO::* Params[] = { &SW_LFO::rate, &SW_LFO::waveform, &SW_LFO::reset, &SW_LFO::slew };

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
    //list of Params that we manage on step()
    static constexpr Param SW_CLOCK::* Params[] = { &SW_CLOCK::rate };
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
private:
    //list of Params that we manage on step()
    static constexpr Param SW_ADSR::* Params[] = { &SW_ADSR::attack, &SW_ADSR::decay, &SW_ADSR::sustain, &SW_ADSR::release };
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
    //list of Params that we manage on step()
    static constexpr Param SW_DA::* Params[] = { &SW_DA::delay, &SW_DA::attack };
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