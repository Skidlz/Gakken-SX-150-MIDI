#include "hw_vcf.h"

void HW_VCF::updateCut(float currentNote, float offset) {
    float newVCFcut = cut + offset;
    if (keyTracking != 0) newVCFcut += (currentNote / 127.0) * keyTracking;

    if (newVCFcut > 1) newVCFcut = 1; //cap at max
    else if (newVCFcut < 0) newVCFcut = 0; //cap at min

    _cutDac.write(newVCFcut);
}

void HW_VCF::updateResonance(float newValue) {
    resonance = newValue;
    _resonancePot.write(newValue);
}

void HW_VCF::setMode(Mode newMode) {
    if (mode == newMode) return;
    mode = newMode;

    //TODO: set switch somehow. If we use a io controller, then Voice needs to control it
};