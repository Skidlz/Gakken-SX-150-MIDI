#include "modulator.h"

SW_LFO::SW_LFO(const char* p) : prefix(p),
        rate  { "Rate", getRateStr, &prefix },
        depth { "Depth", Param::toPercentStr, &prefix },
        waveform { "Waveform", _waveform.getStr, &prefix },
        reset { "Reset", getPhaseStr, &prefix },
        slew { "Slew Time", getSlewStr, &prefix } {
    //TODO: option to skip reset with legato notes
    _waveform.value = TRIANGLE;
    _phase = 0;
    setRate(.5);
    step();
}

void SW_LFO::step() { //progress by one tick
    //update using any dirty params
    if (rate.dirty) setRate(rate.get());
    if (depth.dirty) scale = depth.get();

    _waveform.update(waveform);

    if (slew.dirty) setSlew(slew.get());

    _phase += _stepSize;

    float tempOut = 0;
    switch (_waveform.value) {
        case TRIANGLE:
            tempOut = (_phase < HALF_MAX) ? (float)_phase / HALF_MAX : (float)(MAX - _phase) / HALF_MAX;
            break;
        case SAW:
            tempOut = (float)_phase / MAX;
            break;
        case SQUARE:
            tempOut = (_phase < HALF_MAX) ? 0 : 1;
            break;
        case SINE:
            tempOut = sin(2.0 * M_PI * (float)_phase / MAX) / 2 + .5;
            break;
        case NOISE:
            if (_phase > HALF_MAX && _phase - _stepSize < HALF_MAX)
                _previousValue = random() / (float) RAND_MAX;
            tempOut = _previousValue;
            break;
    }

    //slew rate limit
    if (slew.value > 0 && abs(tempOut - _currentSlewed) > _slewRate)
        _currentSlewed += _slewRate * ((tempOut > _currentSlewed) ? 1 : -1);
    else _currentSlewed = tempOut;

    output = (_currentSlewed * 2) - 1; //±1
}

void SW_LFO::gateOn() { //reset LFO
    if (reset.value) _phase = reset.get() * MAX; //reset control changes reset phase
}

void SW_LFO::setRate(float rate) { //0-1 rate = .01-100Hz
    float freq = MIN_HZ * powf(RANGE, rate);
    _stepSize = (uint32_t) (MAX / TICK_RATE * freq);
}

void SW_LFO::setSlew(float rate) { //0-1 rate = 50-.01Hz
    float freq = MIN_SLEW * powf(SLEW_RANGE, 1 - rate);
    _slewRate = freq / TICK_RATE;
}

const char* SW_LFO::getRateStr(char* buffer, size_t size, uint8_t value) {
    float freq = MIN_HZ * powf(RANGE, value / 127.0);

    char floatBuffer[10]; //buffer for float to string
    const uint8_t decimalPlaces = (freq < 1) ? 3 : (freq < 10) ? 2 : 1;
    dtostrf(freq, 5, decimalPlaces, floatBuffer);
    snprintf(buffer, size, "%6sHz", floatBuffer); //pad number

    return buffer;
}

const char* SW_LFO::getSlewStr(char* buffer, size_t size, uint8_t value) {
    if (value == 0) return "Off";

    float freq = MIN_SLEW * powf(SLEW_RANGE, 1 - (value / 127.0));

    char floatBuffer[10]; //buffer for float to string
    float timeInS = 1 / freq;
    if (timeInS < 1) {
        uint16_t timeInMs = timeInS * 1000;
        snprintf(buffer, size, "%6d ms", timeInMs); //pad number
    } else {
        const uint8_t decimalPlaces = (timeInS < 10) ? 3 : (timeInS < 100) ? 2 : 1;
        dtostrf(timeInS, 5, decimalPlaces, floatBuffer);
        snprintf(buffer, size, "%6s s", floatBuffer); //pad number
    }

    return buffer;
}

const char* SW_LFO::getPhaseStr(char* buffer, size_t size, uint8_t value) {
    return (!value) ? "  0ff" : Param::toPercentStr(buffer, size, value);
}

//Clock--------------------------------------------------------------------------------------------
SW_CLOCK::SW_CLOCK(const char* p) : prefix(p),
        rate  { "S/H Rate", getRateStr, &prefix } {
    _phase = 0;
    setRate(.5);
    step();
}

void SW_CLOCK::step() { //progress by one tick
    if (rate.dirty) setRate(rate.get());

    _phase += _stepSize;

    output = !!(_phase > HALF_MAX);
}

void SW_CLOCK::gateOn() { //reset LFO
    if (sync) _phase = 0;
}

void SW_CLOCK::setRate(float rate) { //0-1 rate = 200-4Hz
    float freq = MIN_HZ * powf(RANGE, (1 - rate)); //low val = higher clock

    _stepSize = (uint32_t)(MAX / TICK_RATE * freq );
}

const char* SW_CLOCK::getRateStr(char* buf, size_t len, uint8_t v) {
    if (!v) return " Off";

    float freq = MIN_HZ * powf(RANGE, (127 - v) / 127.0);

    char floatBuffer[10]; //buffer for float to string
    const uint8_t decimalPlaces = (freq < 1) ? 3 : (freq < 10) ? 2 : 1;
    dtostrf(freq, 5, decimalPlaces, floatBuffer);
    snprintf(buf, len, "%6sHz", floatBuffer); //pad number

    return buf;
}

//ADSR---------------------------------------------------------------------------------------------
SW_ADSR::SW_ADSR(const char* p) : prefix(p),
        attack  { "Attack", Param::toPercentStr, &prefix },
        decay { "Decay", Param::toPercentStr, &prefix },
        sustain { "Sustain", Param::toPercentStr, &prefix },
        release { "Release", Param::toPercentStr, &prefix } {
    _phase = 0;
    _currentStage = RELEASE;
    scale = 1;

    //set up the stages
    _stages[ATTACK] = { 0.005, 3, 0, 0, 0, 1.5 };
    _stages[DECAY] = { 0.005, 5, 0, 0, .5, 0 };
    _stages[RELEASE] = { 0.005, 5, 0, 0, 0, 0 };
    setSustain(.5);

    for (Stage stage: { ATTACK, DECAY, RELEASE }) {
        auto& [minPeriod, maxPeriod, range, alpha, rate, target] = _stages[stage];
        if (minPeriod == 0) minPeriod = .001; //force minimum range
        range = maxPeriod / minPeriod; //precalc range
        setRate(stage, rate);
    }
}

void SW_ADSR::setRate(Stage stage, float newRate) {
    if (stage == SUSTAIN) return setSustain(newRate);

    //get members of specific stage
    auto& [minPeriod, maxPeriod, range, alpha, rate, target] = _stages[stage];
    rate = newRate;
    float period = (newRate == 0) ? MIN_RATE : (minPeriod * powf(range, rate)); //exponentiate period range
    if (period == 0) period = MIN_RATE; //avoid divide by zero

    alpha = 1 - exp(- (TICK_RATE_INV / period));
}

void SW_ADSR::setSustain(float sustain) {
    values[SUSTAIN] = sustain;
    _stages[DECAY].target = sustain;
}

//TODO: pass in gate and add legato option?
void SW_ADSR::gateOn() {
    _currentStage = ATTACK;
    sampHoldClock.gateOn();
}

void SW_ADSR::gateOff() { _currentStage = RELEASE; }

void SW_ADSR::step() {
    auto& [minPeriod, maxPeriod, range, alpha, rate, target] = _stages[_currentStage];

    if (attack.dirty) setRate(ATTACK, attack.get());
    if (decay.dirty) setRate(DECAY, decay.get());
    if (sustain.dirty) setRate(SUSTAIN, sustain.get());
    if (release.dirty) setRate(RELEASE, release.get());

    if (_phase != target) _phase += alpha * (target - _phase);

    if (_currentStage == ATTACK && _phase >= 1) {
        _phase = 1; //remove any overshoot
        _currentStage = DECAY;
    } else if (_currentStage == DECAY && _phase < target) {
        _phase = target; //remove undershoot
    }

    if (!sampHoldClock.rate.value) { //ignore SH if stopped
        output = _phase * scale;
        return;
    }

    //only update output when LFO crosses threshold
    float lfoBefore = sampHoldClock.output;
    sampHoldClock.step();
    if (!lfoBefore && sampHoldClock.output)
        output = _phase * scale;
}

//Delay Attack-------------------------------------------------------------------------------------
SW_DA::SW_DA(const char* p) : prefix(p),
        delay { "Delay", Param::toPercentStr, &prefix },
        attack  { "Attack", Param::toPercentStr, &prefix } {
    _phase = 0;
    _currentStage = ATTACK;
    scale = 1;

    //set up the stages
    _stages[DELAY] = { 0.1, 3, 0, 0, .5 };
    _stages[ATTACK] = { 0.2, 4, 0, 0, .5 };
    _stages[STALL] = { 0, 1, 0, 0, .5 };

    for (Stage stage: { DELAY, ATTACK, STALL }) {
        auto& [minPeriod, maxPeriod, range, alpha, rate] = _stages[stage];
        if (minPeriod == 0) minPeriod = .001; //force minimum range
        range = maxPeriod / minPeriod; //precalc range
        setRate(stage, rate);
    }
}

void SW_DA::setRate(Stage stage, float newRate) {
    //get members of specific stage
    auto& [minPeriod, maxPeriod, range, alpha, rate] = _stages[stage];
    rate = newRate;
    float period = (newRate == 0) ? MIN_RATE : (minPeriod * powf(range, rate)); //exponentiate period range
    if (period == 0) period = MIN_RATE; //avoid divide by zero

    alpha = 1 - exp(- (TICK_RATE_INV / period));
}

void SW_DA::gateOn() {
    _currentStage = DELAY;
    _phase = 0;
}

void SW_DA::gateOff() {  }

void SW_DA::step() { //delay at 0, expo rise to 1 __..'¯¯¯
    if (delay.dirty) setRate(DELAY, delay.get());
    if (attack.dirty) setRate(ATTACK, attack.get());

    switch (_currentStage) {
        case DELAY:
            _phase += _stages[DELAY].alpha;
            if (_phase >= 1) {
                _phase = 0; //restart
                _currentStage = ATTACK;
            }

            output = 0;
            break;
        case ATTACK:
            _phase += _stages[ATTACK].alpha;
            if (_phase >= 1) _currentStage = STALL;

            output = _phase * _phase; //x^2
            break;
        case STALL:
            output = 1;
    }
}