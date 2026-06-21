#pragma once
#include "voice.h"
#include "parameter.h"

extern Voice voice;

namespace NoteAssigner {
    enum NotePriority { LAST_NOTE, LOW_NOTE, HIGH_NOTE, PRIORITY_COUNT };

    extern uint8_t pressedKeys[127]; //hold all currently pressed notes
    extern uint8_t keyCount;
    extern Param notePriority;
    extern const char*  NotePriorityNames[];

    extern EnumParam<NotePriority, PRIORITY_COUNT, NotePriorityNames, 10> _notePriority;

    const char* getPriorityStr(char* buffer, size_t size, uint8_t value);
    uint8_t getPriorityNote(uint8_t note);
    uint8_t *find(uint8_t *first, uint8_t *last, uint8_t value);
    void noteOnHandler(uint8_t note, uint8_t vel);
    void noteOffHandler(uint8_t note, uint8_t vel);
    void update();
}