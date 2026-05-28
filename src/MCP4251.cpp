// MCP4251 digipot library

#include "MCP4251.h"

MCP4251::MCP4251(uint8_t pin) {
    _csPin = pin;
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
}

void MCP4251::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    SPI.begin();
    //Init TCON registers that con connect/disconnect legs
    TCON0 = 0x1FF;
    TCON1 = 0x1FF;
    writeRegister(TCON0_ADDR, TCON0);
    writeRegister(TCON1_ADDR, TCON1);
}

void MCP4251::setWiper(Pot pot, uint8_t position) {
    writeRegister(pot, position);
}

uint8_t MCP4251::getWiper(Pot pot) {
    return readRegister(pot);
}

void MCP4251::disconnectLeg(Pot pot, Leg leg) {
    uint8_t address = (pot < POT2) ? TCON0_ADDR : TCON1_ADDR;
    uint8_t data = (pot < POT2) ? TCON0 : TCON1;
    uint8_t offset = (pot == POT0 || pot == POT2) ? 0 : 4;

    data &= ~(1 << (offset + leg)); //clear bit to disconnect pin

    writeRegister(address, data);
}

void MCP4251::connectLeg(Pot pot, Leg leg) {
    uint8_t address = (pot < POT2) ? TCON0_ADDR : TCON1_ADDR;
    uint8_t data = (pot < POT2) ? TCON0 : TCON1;
    uint8_t offset = (pot == POT0 || pot == POT2) ? 0 : 4;

    data |= (1 << (offset + leg)); //set bit to connect pin

    writeRegister(address, data);
}

bool MCP4251::getLegStatus(Pot pot, Leg leg) {
    uint8_t data = (pot < POT2) ? TCON0 : TCON1;
    uint8_t offset = (pot == POT0 || pot == POT2) ? 0 : 4;

    data &= (1 << (offset + leg)); //test one bit

    return !!(data & (1 << (offset + leg))); //true = enabled, false = disabled
}

void MCP4251::writeRegister(uint8_t address, uint8_t data) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    SPI.transfer((address << 4) | WRITE); //address + command
    SPI.transfer(data);

    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

uint8_t MCP4251::readRegister(uint8_t address) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    SPI.transfer((address << 4) | READ); //address + command
    uint8_t data = SPI.transfer(0xFF);

    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();

    return data;
}