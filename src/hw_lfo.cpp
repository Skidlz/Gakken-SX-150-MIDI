#include "hw_lfo.h"

HW_LFO::HW_LFO(Dac& rateDac) : _rateDac(rateDac) {
    pinMode(RESET_PIN, OUTPUT);
}

void HW_LFO::update() {
    if (rate.dirty) _rateDac.write(rate.get());
    if (waveform.dirty) setWaveform(static_cast<Waveform>(waveform.get() * WAVE_SCALE));
    if (reset.dirty) setResetMode(static_cast<Reset>(reset.get() * RESET_SCALE));
}

void HW_LFO::setResetMode(Reset newMode){
    _resetMode = newMode;
}

void HW_LFO::setWaveform(Waveform waveform){
    //TODO: set waveform pin
}

//TODO: figure out approximate LFO rates
const char* HW_LFO::getRateStr(char* buffer, size_t size, uint8_t value) {
    float freq = MIN_HZ * powf(RANGE, value / 127.0);

    char floatBuffer[10]; //buffer for float to string
    const uint8_t decimalPlaces = (freq < 1) ? 3 : (freq < 10) ? 2 : 1;
    dtostrf(freq, 5, decimalPlaces, floatBuffer);
    snprintf(buffer, size, "%6sHz", floatBuffer); //pad number

    return buffer;
}

const char* HW_LFO::getWaveformStr(char* buffer, size_t size, uint8_t value) {
    snprintf(buffer, size, "%10s", WaveformNames[value * WAVE_COUNT / 128]); //pad string
    return buffer;
}

const char* HW_LFO::getResetStr(char* buffer, size_t size, uint8_t value) {
    snprintf(buffer, size, "%13s", ResetModes[value * RESET_COUNT / 128]); //pad string
    return buffer;
}

//TODO: test
void HW_LFO::gateOn(bool gate) {
    if (_resetMode == FREE_RUN) return; //don't reset

    if (gate) {
        if (_resetMode == LEGATO) return; //don't reset

        digitalWrite(RESET_PIN, LOW); //force reset
        delayMicroseconds(10);
    }

    digitalWrite(RESET_PIN, HIGH);
}

void HW_LFO::gateOff() {
    if (_resetMode == FREE_RUN) return;
    digitalWrite(RESET_PIN, LOW);
}