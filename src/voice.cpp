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

    modSort.reserve(NUM_ROUTES * 4); //allocate enough room for source1, source2, route, and dest owner
}

void Voice::evalRoutes() {
    std::map<Param*, Modulator*> paramToOwner;

    for (Route& route : routes) //find owner of all Params
        for (Param* param : { route.destination, &route.depth })
            if (Modulator* mod = findOwner(param))
                paramToOwner[param] = mod;

    RouteGraph::generateModOrder(routes, routesUsed, paramToOwner, modSort);
}

Modulator* Voice::findOwner(Param* param) {
    if (param == nullptr) return nullptr;

    //search Voice modulators
    for (Modulator* modulator : std::initializer_list<Modulator*>{ &accentADSR, &vcaADSR })
        if (Modulator* owner = modulator->findOwner(param)) return owner;

    //search modulators owned by Osc, etc
    if (Modulator* owner = osc.findOwner(param)) return owner;
    //if (Modulator* owner = vcf.findOwner(param)) return owner;

    return nullptr;
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

void Voice::update() {
    if (glideTime.dirty) setGlideTime(glideTime.get());
    if (gate && glideOn) updateGlide(); //only glide when key(s) held
    osc.note = (currentGlideNote + (currentBend * bendRange));

    //update HW modulators and anything that isn't routed
    vcaADSR.step();
    env.update();

    //Update all modulators in topological order
    for (Modulator* mod : modSort) mod->step();

    //set dummy Modulator to pass in keyTracking
    vcf.keyTrackMod.input = (vcf.keyTracking.value != 0) ? (osc.note / 127) * vcf.keyTracking.get() : 0;

    vcf.update();
    osc.update();
}