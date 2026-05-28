// MCP4251 digipot library
#pragma once

#include "Arduino.h"
#include <SPI.h>

class MCP4251 {
public:
    enum Pot { POT0, POT1, POT2 = 6, POT3 = 7 }; //Including MPC43XX pot addresses too
    enum Leg { B, W, A }; //terminal B, Wiper, terminal A
    MCP4251() { _csPin = 10; } //default CS pin 10
    MCP4251(uint8_t csPin);
    void begin();
    void setWiper(Pot pot, uint8_t position);
    void disconnectLeg(Pot pot, Leg leg);
    void connectLeg(Pot pot, Leg leg);
    bool getLegStatus(Pot pot, Leg leg);
    uint8_t getWiper(Pot pot);
private:
    static constexpr uint8_t TCON0_ADDR = 0x04; //Terminal Control Register 0 address
    static constexpr uint8_t TCON1_ADDR = 0x0A; //Terminal Control Register 1 address
    uint16_t TCON0; //Terminal Control Register 0
    uint16_t TCON1; //Terminal Control Register 1
    enum Command { WRITE, READ = 0x0C };

    uint8_t _csPin; //chip select

    void writeRegister(uint8_t address, uint8_t data);
    uint8_t readRegister(uint8_t address);
};