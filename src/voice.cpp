#include "voice.h"

Voice::Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& vcfDrivePot, DigiPot& pwmPot, Dac& lfoRateDac, HW_adsr& adsr) :
        vcf(vcfCutDac, resPot, vcfDrivePot),
        osc(pwmPot),
        lfo(lfoRateDac),
        env(adsr) {
    setGlideTime(.25);

    env.legato = true;
    osc.legato = true;
    accentADSR.setRate(SW_ADSR::ATTACK, .3);
    accentADSR.setRate(SW_ADSR::DECAY, .4);
    accentADSR.setSustain(0);
}

void Voice::setGlideTime(float time) {
    glideOn = (time != 0); //turn glide off/on if rate = 0 or not
    if (time == 0) return; //don't need to recalc alpha

    time += MIN_GLIDE_TM;
    time *= time; //x^2

    constexpr float CHARGE_99 = 4.61;
    //time in Seconds to 99% = 4.61 / _glideAlpha x TICK_RATE
    _glideAlpha = CHARGE_99 / (TICK_RATE * time); //max of ~1.25 seconds @ 4kHz
    if (_glideAlpha > 1) _glideAlpha = 1;
    else if (_glideAlpha < 0) _glideAlpha = 0;
}

const char* Voice::getGlideTime(char* buffer, size_t size, uint8_t value) {
    if (value == 0) return "Off";

    float time = value / 127.0;
    time += MIN_GLIDE_TM;
    time *= time; //x^2

    uint16_t timeInMs = time * 1000;
    snprintf(buffer, size, "%4d ms", timeInMs); //right aligned
    return buffer;
}

void Voice::updateGlide() {
    if (currentGlideNote == _targetNote) return; //already on target note? exit

    currentGlideNote += _glideAlpha * (_targetNote - currentGlideNote);

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
}

void Voice::updateTargetNote(uint8_t note) {
    currentNote = _targetNote = note;

    if (!glideOn) currentGlideNote = note; //jump to note if no glide

    osc.setNote(currentGlideNote + (currentBend * bendRange)); //apply bend
}

void Voice::noteOn(uint8_t note, uint8_t vel) {
    //turn glide on if more than one note is pressed
    if (gate && glideLegato && glideTime.value > 0) glideOn = true;

    updateTargetNote(note);

    osc.gateOn(gate);
    env.gateOn(gate);
    //TODO: add legato setting in the envelope?
    if (!gate) vcaADSR.gateOn();

    if (vel > accentThreshold) accentADSR.gateOn();

    gate = true;
}

void Voice::noteOff(uint8_t note, uint8_t vel) {
    gate = false;

    accentADSR.gateOff();
    osc.gateOff();
    env.gateOff();vcaADSR.gateOff();

    if (glideLegato) glideOn = false;
}

void Voice::setPitchBend(int16_t bend) {
    currentBend = ((bend / 8192.0) - 1); //±1 semitone

    //bend the current note
    osc.setNote(currentGlideNote + (currentBend * bendRange));
}

void Voice::update() {
    if (glideTime.dirty) setGlideTime(glideTime.get());
    if (gate && glideOn) updateGlide(); //only glide when key(s) held

    accentADSR.step();
    vcaADSR.step();

    env.update();

    float keytrackOffset = (vcf.keyTracking.value != 0) ? (currentNote / 127.0) * vcf.keyTracking.get() : 0.0;
    vcf.cutoff.setMod(accentADSR.output * vcfAccAmt.get() + keytrackOffset);
    vcf.update();
    osc.update();
}