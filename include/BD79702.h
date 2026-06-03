#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "AnalogOutput.h"

class BD79702;
class Dac;

//Parent Chip class
class BD79702 {
public:
    enum Chan { AO1 = 1, AO2 = 2, AO3 = 5, AO4 = 6 }; //Register addresses
    uint8_t csPin; //chip select
    BD79702() { _csPin = 10; } //default CS pin 10
    BD79702(uint8_t csPin) : _csPin(csPin) { }
    void begin();
    void setDAC(Chan chan, uint8_t value);

    Dac getChannel(Chan chan);
private:
    uint8_t _csPin; //chip select
    void writeRegister(uint8_t address, uint8_t data);
    uint8_t readRegister(uint8_t address);
};

struct DacConfig {
    bool invert = false;
    float(*transform)(float) = nullptr;
};

//Child single Dac class
class Dac : public AnalogOutput {
public:
    Dac(BD79702& parent, BD79702::Chan chan, const DacConfig& config = {})
            : _parent(parent), _chan(chan), _config(config) {}

    void write(float position) {
        auto& [invert, transform] = _config;
        if (invert) position *= -1;
        if (transform) position = transform(position);
        _parent.setDAC(_chan, (uint8_t)(position * 255));
    }
private:
    BD79702& _parent; //pointer
    BD79702::Chan _chan;
    DacConfig _config;
};

inline Dac BD79702::getChannel(BD79702::Chan chan) {
    return Dac(*this, chan);
}