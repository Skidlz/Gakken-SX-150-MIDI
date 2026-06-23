#include "voice.h"

Voice::Voice(Dac& vcfCutDac, DigiPot& resPot, DigiPot& vcfDrivePot, DigiPot& pwmPot, Dac& lfoRateDac, HW_ADSR& adsr) :
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

    routes[0].depth.set(64);
    routes[1].depth.set(64);

    buildDestinations(); //figure out what the routes target
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

    float time = (value / 127.0) + MIN_GLIDE_TM;
    time *= time; //x^2

    snprintf(buffer, size, "%5d ms", (uint16_t)(time * 1000)); //right aligned
    return buffer;
}

void Voice::updateGlide() {
    if (currentGlideNote == _targetNote) return; //already on target note? exit

    currentGlideNote += _glideAlpha * (_targetNote - currentGlideNote);
}

void Voice::updateTargetNote(uint8_t note) {
    _targetNote = note;

    if (!glideOn) currentGlideNote = note; //jump to note if no glide

    osc.setNoteHW(currentGlideNote + (currentBend * bendRange)); //apply bend
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
    env.gateOff();
    vcaADSR.gateOff();

    if (glideLegato) glideOn = false;
}

void Voice::setPitchBend(int16_t bend) {
    currentBend = ((bend / 8192.0) - 1); //±1 semitone
}

void Voice::buildDestinations() { //loop over all routes, making a deduped list of destination params
    //TODO: init params to have NaN as their summingNode value
    // if anything clears that, it must be routed.
    // if de-routed, re-assign NaN
    // owner of Param tests summingNode on update, and has to own node if NaN
    //  else only add modulation to it

    destinationCount = 0;
    for (const Route& r : routes) {
        if (!r.destination) continue;

        int8_t index = destinationCount; //only add if not a duplicate
        while (index--) if (destinations[index] == r.destination) break;
        if (index == -1) destinations[destinationCount++] = r.destination;
    }
}

void Voice::update() {
    if (glideTime.dirty) setGlideTime(glideTime.get());
    if (gate && glideOn) updateGlide(); //only glide when key(s) held

    //update modulators
    accentADSR.step();
    vcaADSR.step();
    env.update();

    osc.note = (currentGlideNote + (currentBend * bendRange));

    //Process Routes-------------------------------------------------------------------------------
    //zero summing nodes
    for (size_t i = destinationCount; i--;) destinations[i]->summingNode = 0;

    //base keytracking off of the actual pitch we set the Osc to
    float keytrackOffset = (vcf.keyTracking.value != 0) ? (osc.note / 127) * vcf.keyTracking.get() : 0;
    vcf.cutoff.summingNode = (accentADSR.output * vcfAccAmt.get()) + keytrackOffset;

    //add in any other modulation here:

    for (Route& r : routes) r.step(); //process routes to update nodes

    //update route destination params with summed values
    for (size_t i = destinationCount; i--;) destinations[i]->setMod(destinations[i]->summingNode);

    //debug
    static int debugCount = 0;
    static int delayDebug = 0;
    if (delayDebug++ > 2000 && debugCount++ < 1) {  // print once
        debugCount = 1;

        Serial.print("routes[1] source output (via pointer): ");
        Serial.println(routes[1].source->output);
    }

    vcf.cutoff.setMod(vcf.cutoff.summingNode);

    //TODO: make sure all destinations know to use the new Param.modulation

    vcf.update();
    osc.update();
}