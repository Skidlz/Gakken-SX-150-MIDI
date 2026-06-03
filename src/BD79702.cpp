//Library for BD79702 DAC chip. Note BD79703 has different memory map
#include "BD79702.h"

void BD79702::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    SPI.begin();
}

void BD79702::setDAC(Chan chan, uint8_t value) {
    writeRegister((uint8_t) chan, value);
}

void BD79702::writeRegister(uint8_t address, uint8_t data) {
    //max rate = 40MHz
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    SPI.transfer(address & 0x0F); //address
    SPI.transfer(data);

    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}