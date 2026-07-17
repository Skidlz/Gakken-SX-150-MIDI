#pragma once

#include <Arduino.h>

//simple params lack modulation and can be used for things like global settings
struct SimpleParam {
    const char* name;
    const char* (*getValueStr)(char* buffer, size_t size, uint8_t value) = SimpleParam::toPercentStr;
    const char** prefix = nullptr; //points to prefix string (used for Modulators)

    uint8_t value{}; //raw 7-bit CC value
    bool dirty{}; //flag to say that the value has changed

    static constexpr uint8_t VAL_BUFFER_LEN = 25;

    void set(uint8_t cc) {
        if (value != cc) dirty = true;
        value = cc;
    }

    float get() {
        dirty = false; //assume the value was used to update parent
        return value / 127.0f;
    }

    const char* toString(char* buffer, size_t size) const {
        return getValueStr(buffer, size, value); //used to pass 'value' into string function
    }

    //appends an optional prefix to the params
    const char* fullName(char* buf, size_t size) const {
        if (!(prefix && *prefix)) return name;

        snprintf(buf, size, "%s %s", *prefix, name);
        return buf;
    }

    static const char* toIntStr(char* buffer, size_t size, uint8_t val) {
        snprintf(buffer, size, "%4d", val);
        return buffer;
    }

    static const char* toBoolStr(char* buffer, size_t size, uint8_t val) {
        strncpy(buffer, (val >= 64) ? "  On" : " Off", size);
        return buffer;
    }

    static const char* toPercentStr(char* buffer, size_t size, uint8_t val) {
        float result = val / 1.27; //0 - 100
        char floatBuffer[10]; //buffer for float to string
        dtostrf(result, 4, 1, floatBuffer);
        snprintf(buffer, size, "%5s%%", floatBuffer); //pad number
        return buffer;
    }
};

struct Param : public SimpleParam {
    float modulation = 0; //holds final modulation
    float summingNode = 0; //accumulates modulation from routes

    void commit() { //store and clear summingNode
        if (modulation != summingNode) dirty = true;
        modulation = summingNode;
        summingNode = 0.0; //get ready for another cycle of adding modulation
    }

    float get() {
        float newValue = SimpleParam::get() + modulation;

        if (newValue < 0) newValue = 0; //clip to 0.0-1.0
        if (newValue > 1) newValue = 1;

        return newValue;
    }
};

//helper struct for Enum Params. Makes string function, converts/stores enum
template<typename EnumT, EnumT COUNT, const char* const* Names, int PAD = 0>
struct EnumParam {
    static constexpr float SCALE = 127.0f * COUNT / 128.0f;
    EnumT value{};

    static const char *getStr(char *buffer, size_t size, uint8_t value) {
        snprintf(buffer, size, "%-*s", PAD, Names[static_cast<EnumT>(value * COUNT / 128)]);
        return buffer;
    }

    bool set(float normValue) { //takes 0-1
        if (normValue > 1) normValue = 1;
        else if (normValue < 0) normValue = 0;

        EnumT newValue = static_cast<EnumT>(normValue * SCALE);
        if (newValue == value) return false;

        value = newValue;
        return true;
    }

    bool update(Param& p) { //update enum from param. return true if the *enum* changed
        if (!p.dirty) return false;

        return set(p.get());
    }
};