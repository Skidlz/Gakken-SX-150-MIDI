#include "hw_vco.h"
#include <math.h>

//Library to control modified hardware oscillator in Gakken SX-150

HW_VCO::HW_VCO(DigiPot& pwmPot) : _pwmPot(pwmPot),
        _modulators { &pwmLFO, &pwmADSR, &pwmDA } {
    tuneScaling = 1.0;
    tuningOffset = 0;
    //setNote(12);

    pwm.set(63); //midpoint

    for (uint8_t pin: { SAW_SW, SUB_SW, PUL1_SW, PUL2_SW, TRI_SW })
        pinMode(pin, OUTPUT); //init output pins
    _waveform.value = SAW; //default to Saw
}

//toggles pins to pick waveform
void HW_VCO::setWaveformHW(Waveform waveform) {
    uint8_t config = waveformDefinitions[waveform];

    digitalWrite(SAW_SW, !!(config & saw_b));
    digitalWrite(PUL1_SW, !!(config & sqr1_b));
    digitalWrite(PUL2_SW, !!(config & sqr2_b));
    digitalWrite(SUB_SW, !!(config & sub_b));

    if (config & tri_b) { //put in Hi-z mode to allow Triangle
        //CD4013 output toggles Saw polarity to make Triangle
        pinMode(TRI_SW, INPUT);
    } else { //force pin low to disable Triangle
        pinMode(TRI_SW, OUTPUT);
        digitalWrite(TRI_SW, (config & inv_saw_b) ? LOW : HIGH);
    }
}

//measures oscillator and sets tuning variabels to compensate
void HW_VCO::calibrate() {
    setWaveformHW(NO_WAVE); //silence osc for calibration

    setNoteHW(C2); //play low note
    delay(100); //let it stabilize
    float lowResult = measureFreq();

    setNoteHW(C5); //play high note
    delay(100); //let it stabilize
    float hiResult = measureFreq();

    setWaveformHW(_waveform.value); //restore original waveform

    //slope = rise over run: (y2 - y1) / (x2 - x1)
    tuneScaling = (hiResult - lowResult) / (C5 - C2);
    //find slope intercept (0V frequency): y = mx + b; b = y - mx
    tuningOffset = lowResult - (tuneScaling * (C2 - LOW_NOTE)) - LOW_NOTE;
}

void HW_VCO::setNoteHW(float note) {
    note += pitch.modulation * 32; //arbitrary max modulation depth of +-32 notes
    pitch.dirty = false;

    //truncate note range
    if (note >= LOW_NOTE) note -= LOW_NOTE;
    //if (note >= HIGH_NOTE) note = ((note - HIGH_NOTE) % 12) + (HIGH_NOTE - 12); //transpose to top octave

    uint16_t dacValue = ((note - tuningOffset) / tuneScaling) * (DAC_STEPS / NOTE_RANGE);
    if (dacValue > DAC_STEPS) dacValue = DAC_STEPS;

    analogWrite(A0, dacValue);
}

float HW_VCO::measureFreq() { //returns equiv MIDI note
    readingsIndex = 0;
    readingsComplete = false;

    //TODO: need to timeout if taking too long. try millis
    while(!readingsComplete); //wait for buffer to fill up with new readings

    float freq = getAvgFreq();

    float note = (12 * log2(freq / A4tuning)) + NOTE_A4; //convert from Hz to MIDI note #
    return note;
}

float HW_VCO::getAvgFreq() {
    uint64_t averageReading = 0; //get average of all readings
    for (uint8_t i = 0; i < READINGS_MAX; i++) averageReading += readingsBuffer[i];
    averageReading /= READINGS_MAX;

    return (float)F_CPU/averageReading; //convert from timer count difference to frequency in Hz
}

//interrupt service routine for measuring Osc frequency
void HW_VCO::captureISR() {
    static uint32_t lastTS = 0; //previous input capture reading
    static uint32_t currentTS = 0;

    lastTS = currentTS;
    currentTS = R_GPT0->GTCCR[0]; //read from input capture register
    lastDelta = currentTS - lastTS;

    readingsBuffer[readingsIndex++] = lastDelta;
    if (readingsIndex >= READINGS_MAX) {
        readingsComplete = true;
        readingsIndex %= READINGS_MAX; //wrap around at max
    }

    //clear interrupt flag for capture compare match A
    R_ICU->IELSR_b[IRQn_CCMPA].IR = 0;
}

void HW_VCO::initTimer() { //hardware timer setup
    static bool timerInitialized = false;

    if (timerInitialized) return; //only run once regardless of osc count

    //Timer Setup------------------------------------------------------------------------------------
    R_MSTP->MSTPCRD_b.MSTPD5 = 0; //enable GPT0 module clock (General PWM Timer 321 to 320 Module Stop)
    delayMicroseconds(10);

    R_GPT0->GTCR_b.CST = 0; //stop timer
    R_GPT0->GTCR_b.MD = 0b000; //saw-wave PWM mode (000b)

    R_GPT0->GTUDDTYC = 0b11; //set 11b first (per datasheet Figure 22.17)
    R_GPT0->GTUDDTYC = 0b01; //01b for up-counting

    //R_GPT0->GTCR_b.TPCS = 0b011; //PCLKD/64 prescale
    R_GPT0->GTPR = 0xFFFFFFFF; //max cycle
    R_GPT0->GTCNT = 0; //0 initial count

    //GTCCRA input capture enabled on the...
    R_GPT0->GTICASR_b.ASCARBL = 1; //rising edge of GTIOCA input when GTIOCB input is 0
    R_GPT0->GTICASR_b.ASCARBH = 1; //rising edge of GTIOCA input when GTIOCB input is 1

    R_GPT0->GTCR_b.CST = 1; //start count operation

    //GPIO Pin D7 (P107) Setup-----------------------------------------------------------------------
    R_PFS->PORT[1].PIN[7].PmnPFS_b.PMR = 1; //Used as an I/O port for peripheral functions
    R_PFS->PORT[1].PIN[7].PmnPFS_b.PSEL = 0b11; //GTIOC0A (GPT peripheral function)

    //Interrupt Setup--------------------------------------------------------------------------------
    //assign GPT0 Capture to IRQn_CCMPA (AGT0_INT_IRQn #17)
    R_ICU->IELSR_b[IRQn_CCMPA].IELS = ELC_EVENT_GPT0_CAPTURE_COMPARE_A;
    NVIC_SetVector(IRQn_CCMPA, (uint32_t)&HW_VCO::captureISR); //point to the ISR function
    NVIC_SetPriority(IRQn_CCMPA, 12);
    NVIC_EnableIRQ(IRQn_CCMPA);

    //TODO: setup timer overflow interrupt?

    timerInitialized = true;
}

void HW_VCO::setPWMhw(float newValue) {
    _pwmPot.write(newValue / 2); //duty cycle only needs to go from 0-50%
}

void HW_VCO::update() {
    for (Param* param : { &waveform, &pwm, &pitch }) param->commit();

    if (pwm.dirty) setPWMhw(pwm.get());

    //change the hardware if the waveform changed
    if (_waveform.update(waveform)) setWaveformHW(_waveform.value);

    //if (pitch.dirty)
    //TODO: need dirty logic for note, or pitch dirty doesn't matter
    setNoteHW(note);
}

void HW_VCO::gateOn(bool gate) {
    if (gate) {
        if (legato) return; //don't retrigger

        for (Modulator* m : _modulators) m->gateOff();
    }

    for (Modulator* m : _modulators) m->gateOn();
}

void HW_VCO::gateOff() {
    for (Modulator* m : _modulators) m->gateOff();
}

Modulator * HW_VCO::findOwner(Param* param) {
    if (param == nullptr) return nullptr;

    for (Modulator* modulator : _modulators)
        if (Modulator* owner = modulator->findOwner(param)) return owner;

    return nullptr;
}