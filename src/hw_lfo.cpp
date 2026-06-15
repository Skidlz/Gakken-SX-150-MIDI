#include "hw_lfo.h"

void HW_LFO::update() {
    if (rate.dirty) _rateDac.write(rate.get());

    if (waveform.dirty) {
        //TODO: set bit to change waveform
    }
}

//TODO: figure out approximate LFO rates
const char* HW_LFO::getRateStr(char* buf, size_t len, uint8_t v) {
    float freq = MIN_HZ * pow(RANGE, v / 127.0);

    char floatBuffer[10]; //buffer for float to string
    const uint8_t decimalPlaces = (freq < 1) ? 3 : (freq < 10) ? 2 : 1;
    dtostrf(freq, 5, decimalPlaces, floatBuffer);
    snprintf(buf, len, "%6sHz", floatBuffer); //pad number

    return buf;
}

const char* HW_LFO::getWaveformStr(char* buffer, size_t size, uint8_t value) {
    snprintf(buffer, 25, "%21s", WaveformNames[value * WAVE_COUNT / 128]); //pad string
    return buffer;
}