#include "voice.h"

Voice::Voice() {
    setGlideRate(.25);
    glideOn = false;
    //glideLegato = true;
    glideBlend = .5; //mix of fixed-rate and fixed-time portamento
    currentGlideNote = osc.NOTE_A4;
    currentNote = osc.NOTE_A4;
    bendRange = 2; //±2 semitone
    currentBend = 0;
    modDepth = 0;
}

void Voice::setGlideRate(float rate) {
    glideOn = (rate != 0); //turn glide off/on if rate = 0 or not

    rate += 15 / 127.0; //minimum rate = 15
    rate *= rate; //x^2

    glideAlpha = 1 / (4000.0 * rate / 5); //max of ~3 seconds @ 4kHz
    if (glideAlpha > 1) glideAlpha = 1;
    if (glideAlpha < 0) glideAlpha = 0;

    //glideRate = pow(rate, 3); //scale exponentially
}

void Voice::updateGlide() {
    if (currentGlideNote == targetNote || glideStep == 0) return; //already on target note? exit

    //currentGlideNote += glideStep;

    currentGlideNote += glideAlpha * (targetNote - currentGlideNote);

    if ((glideStep > 0) && !(currentGlideNote < targetNote) || ((glideStep < 0) && (currentGlideNote < targetNote)))
        currentGlideNote = targetNote; //overshot target, snap to it

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
}

void Voice::noteOn(uint8_t note, uint8_t vel) {
    currentNote = targetNote = note;
    if (!glideOn) currentGlideNote = note; //jump to note if no glide
    else { //calculate new glide rate, blending fixed time and fixed interval rate w/ weighted average
        float fixedTimeInt = (targetNote - currentGlideNote);
        float fixedRateInt = 12.0 * ((fixedTimeInt > 0) ? 1 : -1); //point in right direction
        //rearranged version of: a(1-weight) + b(weight) = a - a*weight + b*weight = a - weight(a - b)
        float weightedInterval = fixedTimeInt - glideBlend * (fixedTimeInt - fixedRateInt);
        glideStep = weightedInterval / (glideRate * 4000); //4 is a multiplier based on update rate
    }

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
    osc.start();
}

void Voice::noteOff(uint8_t note, uint8_t vel) {
    osc.stop();
}

void Voice::setPitchBend(int16_t bend) {
    currentBend = ((bend / 8192.0) - 1); //±1 semitone

    //bend the current note
    osc.setNote(currentGlideNote + (currentBend * bendRange));
}


void Voice::setParams(uint8_t cc, uint8_t val) {
    switch (cc) {
        case 1:

            break;
        default:
            break;
    }
}