#include "noteAssigner.h"


namespace NoteAssigner {
    uint8_t pressedKeys[127] = {0}; //hold all currently pressed notes
    //allows us to fall back to previously pressed note after release
    uint8_t keyCount = 0;
    NotePriority notePri = LAST_NOTE;

    const char*  NotePriorityNames[] = { [LAST_NOTE] = "Last Note", [LOW_NOTE] = "Low Note", [HIGH_NOTE] = "High Note" };

    Param notePriority { "Note Priority", getPriorityStr };

    const char* getPriorityStr(char* buffer, size_t size, uint8_t value) {
        snprintf(buffer, size, "%10s", NotePriorityNames[value * PRIORITY_COUNT / 128]); //pad string
        return buffer;
    }

    uint8_t getPriorityNote(uint8_t note) {
        if (notePri == LAST_NOTE) return note;
        else if (notePri == LOW_NOTE)
            return *std::min_element(pressedKeys, pressedKeys + keyCount);
        else if (notePri == HIGH_NOTE)
            return *std::max_element(pressedKeys, pressedKeys + keyCount);

        return note;
    }

    //find item in array
    uint8_t *find(uint8_t *first, uint8_t *last, uint8_t value) {
        for (; first != last; first++) if (*first == value) return first;

        return last;
    }

    void noteOnHandler(uint8_t note, uint8_t vel) {
        //if (note < voice.osc.LOW_NOTE || note > (osc.LOW_NOTE + 48)) return; //invalid note

        //if (note > voice.osc.LOW_NOTE) note -= osc.LOW_NOTE;

        //put key into array if it's not there already
        uint8_t *arrEnd = pressedKeys + keyCount + 1;
        if (find(pressedKeys, arrEnd, note) == arrEnd && keyCount < 127)
            pressedKeys[keyCount++] = note;

        voice.noteOn(getPriorityNote(note), vel);
    }

    void noteOffHandler(uint8_t note, uint8_t vel) {
        //if (note < voice.osc.LOW_NOTE || note > (osc.LOW_NOTE + 48)) return; //invalid note

        //note -= voice.osc.LOW_NOTE;

        //remove key from array if it's there
        uint8_t *arrEnd = pressedKeys + keyCount + 1;
        uint8_t *keyPtr = find(pressedKeys, arrEnd, note);
        if (keyPtr != arrEnd) { //found
            for (; keyPtr < arrEnd; keyPtr++) //fill in hole
                *keyPtr = *(keyPtr + 1);

            keyCount--;
        }

        if (keyCount > 0) //fallback to last note
            voice.updateTargetNote(getPriorityNote(pressedKeys[keyCount - 1]));
        else
            voice.noteOff(getPriorityNote(pressedKeys[keyCount - 1]), vel);
    }

    void update() {
        constexpr float PRI_SCALE = 127.0 * PRIORITY_COUNT / 128.0;
        if (notePriority.dirty) notePri = static_cast<NotePriority>(notePriority.get() * PRI_SCALE);
    }
}