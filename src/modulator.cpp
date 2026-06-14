#include "modulator.h"

SW_LFO::SW_LFO(const char* p) : prefix(p),
        rate  { "Rate", getRateStr, &prefix },
        depth { "Depth", Param::toPercentStr, &prefix },
        waveform { "Waveform", getWaveformStr, &prefix },
        reset { "Reset", getPhaseStr, &prefix } {
    //TODO: fix Reset readout to show off or phase percentage
    //idea, don't reset with legato notes
    _waveform = TRIANGLE;
    _phase = 0;
    setRate(.5);
    step();
}

void SW_LFO::step() { //progress by one tick
    //update using any dirty params
    if (rate.dirty) setRate(rate.get());
    if (depth.dirty) scale = depth.get();
    constexpr float WAVE_SCALE = 127.0 * WAVE_COUNT / 128.0;
    if (waveform.dirty)
        setWaveform(static_cast<Waveform>(waveform.get() * WAVE_SCALE));

    _phase += _stepSize;

    float tempOut = 0;

    switch (_waveform) {
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
    }

    //+- 1 when scale = 1, offset = 0
//    output = (tempOut * 2 * scale) - 1 + offset; //scale and offset
    output = (tempOut * 2) - 1;
}

void SW_LFO::gateOn() { //reset LFO
    if (reset.value) _phase = reset.get() * MAX; //reset control changes reset phase
}

void SW_LFO::setRate(float rate) { //0-1 rate = .01-100Hz
    float freq = MIN_HZ * pow(RANGE, rate);

    _stepSize = (uint32_t) (MAX / TICK_RATE * freq);
}

const char* SW_LFO::getRateStr(char* buf, size_t len, uint8_t v) {
    float freq = MIN_HZ * pow(RANGE, v / 127.0);

    char floatBuffer[10]; //buffer for float to string
    const uint8_t decimalPlaces = (freq < 1) ? 3 : (freq < 10) ? 2 : 1;
    dtostrf(freq, 5, decimalPlaces, floatBuffer);
    snprintf(buf, len, "%6sHz", floatBuffer); //pad number

    return buf;
}

void SW_LFO::setWaveform(Waveform waveform) {
    _waveform = waveform;
}

const char* SW_LFO::getWaveformStr(char* buffer, size_t size, uint8_t value) {
    snprintf(buffer, 25, "%21s", WaveformNames[value * WAVE_COUNT / 128]); //pad string
    return buffer;
}

//
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
    float freq = MIN_HZ * pow(RANGE, (1 - rate)); //low val = higher clock

    _stepSize = (uint32_t)(MAX / TICK_RATE * freq );
}

const char* SW_CLOCK::getRateStr(char* buf, size_t len, uint8_t v) {
    if (!v) return " Off";

    float freq = MIN_HZ * pow(RANGE, (127 - v) / 127.0);

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
    offset = 0;

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
    float period = (newRate == 0) ? MIN_RATE : (minPeriod * pow(range, rate)); //exponentiate period range
    if (period == 0) period = MIN_RATE; //avoid divide by zero

    alpha = 1 - exp(- (TICK_RATE_INV / period));
}

void SW_ADSR::setSustain(float sustain) {
    values[SUSTAIN] = sustain;
    _stages[DECAY].target = sustain;
}

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
        output = _phase * scale + offset;
        return;
    }

    //only update output when LFO crosses threshold
    float lfoBefore = sampHoldClock.output;
    sampHoldClock.step();
    if (!lfoBefore && sampHoldClock.output)
        output = _phase * scale + offset;
}

//Delay Attack-------------------------------------------------------------------------------------
SW_DA::SW_DA() { //delay at 0, expo rise to 1 __..'¯¯¯
    _phase = 0;
    _currentStage = ATTACK;
    scale = 1;
    offset = 0;

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
    float period = (newRate == 0) ? MIN_RATE : (minPeriod * pow(range, rate)); //exponentiate period range
    if (period == 0) period = MIN_RATE; //avoid divide by zero

    alpha = 1 - exp(- (TICK_RATE_INV / period));
}

void SW_DA::gateOn() {
    _currentStage = DELAY;
    _phase = 0;
}

void SW_DA::gateOff() {  }

void SW_DA::step() {
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