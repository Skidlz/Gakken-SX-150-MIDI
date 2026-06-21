#include "hw_vcf.h"

void HW_VCF::updateCutHW(float newCut) {
    _cutoffDac.write(newCut);
}

void HW_VCF::setModeHW(Mode newMode) {
    //TODO: set switch somehow. If we use a io controller, then Voice needs to control it
};

void HW_VCF::update() {
    if (_cutoff.update(cutoff)) updateCutHW(_cutoff.value);
    if (_mode.update(mode)) setModeHW(_mode.value);

    if (resonance.dirty) _resonancePot.write(resonance.get());
    if (drive.dirty) _drivePot.write(drive.get());
}