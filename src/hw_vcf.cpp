#include "hw_vcf.h"

void HW_VCF::updateCut(float currentNote, float offset) {
    float newVCFcut = cutoff.get() + offset;
    if (keyTracking.value != 0)
        newVCFcut += (currentNote / 127.0) * keyTracking.get();

    if (newVCFcut > 1) newVCFcut = 1; //cap at max
    else if (newVCFcut < 0) newVCFcut = 0; //cap at min

    _cutoffDac.write(newVCFcut);
}

void HW_VCF::setMode(Mode newMode) {
    if (mode == newMode) return;
    mode = newMode;

    //TODO: set switch somehow. If we use a io controller, then Voice needs to control it
};

void HW_VCF::update(float currentGlideNote, float offset) {
    updateCut(currentGlideNote, offset);

    if (resonance.dirty) _resonancePot.write(resonance.get());
    if (drive.dirty) _drivePot.write(drive.get());
}