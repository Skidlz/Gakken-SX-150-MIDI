#include "voice.h"

Voice::Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& pwmPot) : vcf(vcfCutDac, resPot), osc(pwmPot) {
    setGlideRate(.25);
}

void Voice::setGlideRate(float rate) {
    _glideRate = rate;
    glideOn = (rate != 0); //turn glide off/on if rate = 0 or not

    rate += 15 / 127.0; //minimum rate = 15
    rate *= rate; //x^2

    _glideAlpha = 1 / (TICK_RATE * rate / 5); //max of ~3 seconds @ 4kHz
    if (_glideAlpha > 1) _glideAlpha = 1;
    else if (_glideAlpha < 0) _glideAlpha = 0;
}

void Voice::updateGlide() {
    if (currentGlideNote == _targetNote) return; //already on target note? exit

    currentGlideNote += _glideAlpha * (_targetNote - currentGlideNote);

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
}

void Voice::noteOn(uint8_t note, uint8_t vel) {
    currentNote = _targetNote = note;

    //turn glide on if more than one note is pressed
    if (gate && glideLegato && _glideRate > 0.0) glideOn = true;
    if (!glideOn) currentGlideNote = note; //jump to note if no glide
    gate = true;

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
    osc.start();

    osc.pwmADSR.gateOn();
    osc.pwmDA.gateOn();

    if (vel > accentThreshold) accentADSR.gateOn();
    vcf.updateCut(currentGlideNote, accentADSR.output * 0.25);
}

void Voice::noteOff(uint8_t note, uint8_t vel) {
    gate = false;
    osc.stop();
    //TODO: make array of envelopes to turn off
    osc.pwmADSR.gateOff();
    accentADSR.gateOff();

    if (glideLegato) glideOn = false;
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

void Voice::update() {
    if (gate && glideOn) updateGlide(); //only glide when key(s) held

    accentADSR.step();
    //TODO: add accent envelope > vcf amount control
    vcf.updateCut(osc.currentNote, accentADSR.output * 0.25);

    //TODO: add a function to update all osc modulators?
    osc.pwmLFO.step();
    osc.pwmDA.step();
    osc.updatePWM(0);
}