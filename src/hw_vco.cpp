#include "hw_vco.h"
#include <math.h>

//Library to control modified hardware oscillator in Gakken SX-150

Oscillator::Oscillator(DigiPot& pwmPot) : _pwmPot(pwmPot) {
    currentNote = 0;
    running = false;
    tuneScaling = 1.0;
    tuningOffset = 0;
    //setNote(12);

    for (uint8_t pin: { GATE_PIN, SAW_SW, SUB_SW, PUL1_SW, PUL2_SW, TRI_SW })
        pinMode(pin, OUTPUT); //init output pins
    setWaveform(SAW); //default to Saw
}

//toggles pins to pick waveform
void Oscillator::setWaveform(Waveform waveform) {
    _waveform = waveform;
    uint8_t config = waveformConfig[_waveform];

    digitalWrite(SAW_SW, !!(config & saw_b));
    digitalWrite(PUL1_SW, !!(config & pulse1_b));
    digitalWrite(PUL2_SW, !!(config & pulse2_b));
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
void Oscillator::calibrate() {
    Waveform waveformBackup = _waveform;
    setWaveform(NO_WAVE); //silence osc for calibration

    start(); //turn osc on
    setNote(C2); //play low note
    delay(100); //let it stabilize
    float lowResult = measureFreq();

    setNote(C5); //play high note
    delay(100); //let it stabilize
    float hiResult = measureFreq();
    stop(); //turn osc off

    setWaveform(waveformBackup); //restore original waveform

    //slope = rise over run: (y2 - y1) / (x2 - x1)
    tuneScaling = (hiResult - lowResult) / (C5 - C2);
    //find slope intercept (0V frequency): y = mx + b; b = y - mx
    tuningOffset = lowResult - (tuneScaling * (C2 - LOW_NOTE)) - LOW_NOTE;
}

void Oscillator::setNote(float note) {
    //truncate note range
    if (note >= LOW_NOTE) note -= LOW_NOTE;
    //if (note >= HIGH_NOTE) note = ((note - HIGH_NOTE) % 12) + (HIGH_NOTE - 12); //transpose to top octave

    uint16_t dacValue = ((note - tuningOffset) / tuneScaling) * (DAC_STEPS / NOTE_RANGE);
    if (dacValue > DAC_STEPS) dacValue = DAC_STEPS;

    analogWrite(A0, dacValue);
    currentNote = note;
}

void Oscillator::start() {
    digitalWrite(GATE_PIN, HIGH);
    running = true;
}

void Oscillator::stop() {
    digitalWrite(GATE_PIN, LOW);
    running = false;
}

float Oscillator::measureFreq() { //returns equiv MIDI note
    readingsIndex = 0;
    readingsComplete = false;

    //TODO: need to timeout if taking too long. try millis
    while(!readingsComplete); //wait for buffer to fill up with new readings

    float freq = getAvgFreq();

    float note = (12 * log2(freq / A4tuning)) + NOTE_A4; //convert from Hz to MIDI note #
    return note;
}

float Oscillator::getAvgFreq() {
    uint64_t averageReading = 0; //get average of all readings
    for (uint8_t i = 0; i < READINGS_MAX; i++) averageReading += readingsBuffer[i];
    averageReading /= READINGS_MAX;

    return (float)F_CPU/averageReading; //convert from timer count difference to frequency in Hz
}

//interrupt service routine for measuring Osc frequency
void Oscillator::captureISR() {
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

void Oscillator::initTimer() { //hardware timer setup
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
    NVIC_SetVector(IRQn_CCMPA, (uint32_t)&Oscillator::captureISR); //point to the ISR function
    NVIC_SetPriority(IRQn_CCMPA, 12);
    NVIC_EnableIRQ(IRQn_CCMPA);

    //TODO: setup timer overflow interrupt?

    timerInitialized = true;
}


void Oscillator::updatePWM(float offset) {
    //DA envelope fade in LFO
    float adjustedLFO = (pwmLFO.output * pwmDA.output * pwmLFO.scale) / 2;
    float newPWM = pulseWidth + adjustedLFO + offset;

    _pwmPot.write(newPWM / 2); //max
}