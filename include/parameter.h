#pragma once

#include <Arduino.h>
#include <functional>
#include <cstring>

struct Param {
    const char *name;
    const char * (*getValueStr)(char * buffer, size_t size, uint8_t value);
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