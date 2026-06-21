#pragma once

#include <Arduino.h>
#include <cstring>

struct Param {
    const char *name;
    const char * (*getValueStr)(char * buffer, size_t size, uint8_t value) = Param::toPercentStr;
    const char** prefix = nullptr; //points to prefix string (used for Modulators)

    uint8_t value{}; //raw 7-bit CC value
    bool dirty{}; //flag to say that the value has changed
    float modulation = 0; //used to pass in external modulation

    static constexpr uint8_t VAL_BUFFER_LEN = 25;

    void set(uint8_t cc) {
        if (value != cc) dirty = true;
        value = cc;
    }

    void setMod(float m) {
        if (modulation != m) dirty = true;
        modulation = m;
    }

    float get() {
        dirty = false; //assume the value was used to update parent
        return value / 127.0;
    }

    const char * toString(char * buffer, size_t size) const {
        return getValueStr(buffer, size, value); //pass value to function
    }

    //appends an optional prefix to the params
    const char* fullName(char* buf, size_t size) const {
        if (!(prefix && *prefix)) return name;

        snprintf(buf, size, "%s %s", *prefix, name);
        return buf;
    }

    //static string formatting functions----------------------------
    static const char* toIntStr(char* buffer, size_t size, uint8_t val) {
        std::strcpy(buffer, std::to_string(val).c_str());
        snprintf(buffer, size, "%4d", val); //pad number
        return buffer;
    }

    static const char* toBoolStr(char* buffer, size_t size, uint8_t val) {
        std::strncpy(buffer, (val >= 64) ? "  On" : " Off", size);
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

        return set(p.get() + p.modulation);
    }
};

//helper struct that combines Param values and detects changes
struct FloatParam {
    float value = 0.0f;

    bool update(Param& p) { //combine value and modulation. return true if value changed
        if (!p.dirty) return false;
        float newValue = p.get() + p.modulation;

        if (newValue < 0.0f) newValue = 0.0f;
        if (newValue > 1.0f) newValue = 1.0f;
        if (newValue == value) return false;

        value = newValue;
        return true;
    }
};