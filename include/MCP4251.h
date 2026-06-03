// MCP4251 digipot library
#pragma once

#include "Arduino.h"
#include <SPI.h>
#include "AnalogOutput.h"
#include <functional>

class MCP4251;
class DigiPot;

struct DigiPotConfig {
    bool invert = false;
    bool disconnectAtMin = false; //helps isolate signals when at 0
    std::function<float(float)> transform = nullptr; //callback to scale/offset/warp the position
};

//Parent Chip class
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

    DigiPot getChannel(Pot pot, const DigiPotConfig &config = {});
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

//Child single DigiPot class
class DigiPot : public AnalogOutput {
public:
    DigiPot(MCP4251& parent, MCP4251::Pot pot, const DigiPotConfig& config)
            : _parent(parent), _pot(pot), _config(config) {}

    void write(float position) {
        auto& [invert, disconnectAtMin, transform] = _config;
        if (disconnectAtMin) {
            using Leg = MCP4251::Leg;
            Leg leg = (invert)? Leg::B : Leg::A; //always pick leg opposite the "min" leg

            if (position <= 0.001) { //at min
                _parent.disconnectLeg(_pot, leg);
                return; //nothing more to do if disconnected
            } else if (_parent.getLegStatus(_pot, leg))
                _parent.connectLeg(_pot, leg); //reconnect leg
        }

        if (transform) position = transform(position); //callback

        if (invert) position = 1.0 - position;

        _parent.setWiper(_pot, (uint8_t)(position * 255));
    }
private:
    MCP4251& _parent; //pointer
    MCP4251::Pot _pot;
    DigiPotConfig _config;
};

inline DigiPot MCP4251::getChannel(Pot pot, const DigiPotConfig& config) {
    return DigiPot(*this, pot, config);
}