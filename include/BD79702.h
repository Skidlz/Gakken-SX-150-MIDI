#pragma once

#include <Arduino.h>
#include <SPI.h>

class BD79702 {
public:
    enum CHAN { AO1 = 1, AO2 = 2, AO3 = 5, AO4 = 6 }; //Register addresses
    uint8_t csPin; //chip select
    BD79702() { _csPin = 10; } //default CS pin 10
    BD79702(uint8_t csPin) : _csPin(csPin) { }
    void begin();
    void setDAC(CHAN chan, uint8_t value);

private:
    uint8_t _csPin; //chip select
    void writeRegister(uint8_t address, uint8_t data);
    uint8_t readRegister(uint8_t address);
};
