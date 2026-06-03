#pragma once

//abstract class for DACs & Digipots
class AnalogOutput {
public:
    virtual ~AnalogOutput() = default;
    virtual void write(float val) = 0;
};