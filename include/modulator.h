#pragma once
#include "Arduino.h"

class Modulator {
public:
    virtual void step();
    virtual void gateOn();
    float output = 0;
    float scale = 1;
    float offset = 0;

private:

protected: //consts for child classes
    static constexpr float TICK_RATE = 4000.0f;
    static constexpr float TICK_RATE_INV = 1/TICK_RATE;
};

//LFO--------------------------------------------------------------------------
class SW_LFO: public Modulator {
public:
    enum Waveform { TRIANGLE, SQUARE, SINE, SAW };
    SW_LFO();
    void step();
    void gateOn();
    void setRate(float rate);
    void setWaveform(Waveform waveform);
    float output = 0;
    bool sync = false;

private:
    static constexpr uint32_t MAX = UINT32_MAX; //max count
    static constexpr uint32_t HALF_MAX = MAX / 2;
    static constexpr float MIN_HZ = 0.01f;
    static constexpr float MAX_HZ = 100.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    float _rate = .01;
    uint32_t _stepSize = 1;
    Waveform _waveform;
    uint32_t _phase = 0;
};

//Clock-------------------------------------------------------------------------
class SW_CLOCK: public Modulator {
public:
    SW_CLOCK();
    void step();
    void gateOn();
    void setRate(float rate);
    bool output = false;
    bool sync = false;

private:
    static constexpr uint32_t MAX = UINT32_MAX; //max count
    static constexpr uint32_t HALF_MAX = MAX / 2;
    static constexpr float MIN_HZ = 4.0f;
    static constexpr float MAX_HZ = 200.0f;
    static constexpr float RANGE = MAX_HZ / MIN_HZ; //precalc LFO range

    float _rate = 1;
    uint32_t _stepSize = 1;
    uint32_t _phase = 0;
};

//ADSR-------------------------------------------------------------------------
class SW_ADSR: public Modulator {
public:
    //PWM stages are first, then sustain
    enum Stage { ATTACK, DECAY, RELEASE, SUSTAIN, STAGE_COUNT};
    //enum Mode { ADSR, ASR, AD };
    SW_ADSR();
    void step();
    void setRate(Stage stage, float newRate);
    void setSustain(float sustain);

    void gateOn();
    void gateOff();

    float output = 0;

    SW_CLOCK sampHoldClock;
    bool sampleHold = false;

    float values[STAGE_COUNT];
private:
    struct stage {
        float minPeriod;
        float maxPeriod;
        float range;

        float alpha; //used in RC curve math
        float rate;
        float target; //value to move towards
    };

    static constexpr float MIN_RATE = .0001;

    stage _stages[3]; //hold all PWM stages
    Stage _currentStage;

    float _phase = 0;
};

//Delay Attack-----------------------------------------------------------------
class SW_DA: public Modulator {
public:
    //PWM stages are first, then sustain
    enum Stage { DELAY, ATTACK, STALL };
    SW_DA();
    void step();
    void setRate(Stage stage, float newRate);

    void gateOn();
    void gateOff();

    float output = 0;

    float values[2];
private:
    struct stage {
        float minPeriod;
        float maxPeriod;
        float range;

        float alpha; //step size
        float rate;
    };

    static constexpr float MIN_RATE = .0001;

    stage _stages[3]; //hold all PWM stages
    Stage _currentStage;

    float _phase = 0;
};