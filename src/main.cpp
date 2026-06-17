#include <Arduino.h>
//#include <U8g2lib.h>
#include "FspTimer.h"
#include "midi.h"
#include "hw_vco.h"
#include "hw_adsr.h"
#include "voice.h"
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC
#include "analogInputs.h"
#include "parameter.h"

constexpr float x = 0.51; //sets the middle of the curve on x-axis
constexpr float k = 3.8; //sets the curve severity
//x0: sets middle of the curve on x-axis, k: sets curve steepness
std::function<float(float)> makeSCurveClosure(float x0, float k) {
    //precompute offset and denominator
    float offset = 1 / (1 + exp(-k * (0 - x0)));
    float scale = 1 / (1 / (1 + exp(-k * (1 - x0))) - offset);

    //return lambda that will apply s-curve to compensate for imperfect log approximation circuit
    return [x0, k, offset, scale](float position) {
        float sCurve = 1 / (1 + exp(-k * (position - x0)));
        return (sCurve - offset) * scale;
    };
}

//Hardware IO--------------------------------------------------------------------------------------
BD79702 DAC0(8);
Dac sustainDac = DAC0.getChannel(BD79702::AO1);
Dac lfoRateDac = DAC0.getChannel(BD79702::AO2);
Dac vcfCutDac = DAC0.getChannel(BD79702::AO3);
Dac vcaDac = DAC0.getChannel(BD79702::AO4);

MCP4251 digiPot1; //50k
DigiPot lfoToPitchPot = digiPot1.getChannel(MCP4251::POT0, { true, true, makeSCurveClosure(0.51, 3.8) });
DigiPot envToPitchPot = digiPot1.getChannel(MCP4251::POT1, { .invert = false, .disconnectAtMin = true });
DigiPot pwmPot = digiPot1.getChannel(MCP4251::POT2, { .invert = true });
DigiPot lfoToVcfPot = digiPot1.getChannel(MCP4251::POT3, { .invert = false, .disconnectAtMin = true });

MCP4251 digiPot2(9); //10k
DigiPot envToVcfPot = digiPot2.getChannel(MCP4251::POT0, { .invert = true });
DigiPot resonancePot = digiPot2.getChannel(MCP4251::POT1, { .invert = true });
DigiPot volumePot = digiPot2.getChannel(MCP4251::POT2);
DigiPot vcfDrivePot = digiPot2.getChannel(MCP4251::POT3, { .invert = true });

HW_adsr adsr(D3, D5, D4, sustainDac);

touchStrip touchStrip;

Voice voice(vcfCutDac, resonancePot, vcfDrivePot, pwmPot, lfoRateDac, adsr);

//timer
FspTimer tickTimer; //GPT4
bool stepFlag = false; //modulation tick
void tickClock(timer_callback_args_t *args) { stepFlag = true; }

//128x32 I2C OLED on pins A4 (data) & A5 (clock)
//U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
//const char *const noteNames[12] = { "A", "A#/Bb", "B", "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab" };

//MIDI callbacks-----------------------------------------------------
MIDI midi;
void noteOnHandler(uint8_t note, uint8_t vel);
void noteOffHandler(uint8_t note, uint8_t vel);
void midiCcHandler(uint8_t key, uint8_t ptr);

uint8_t pressedKeys[127]; //hold all currently pressed notes
//allows us to fall back to previously pressed note after release
uint8_t keyCount = 0;

void setup() {
    Serial1.begin(31250); //MIDI baud
    Serial.begin(115200); //debug port
    //delay(1000); //let serial connect

    //Terminal needs RTS/CTS flow to work!
    //while (!Serial) {} //wait for debug serial port to connect

    Oscillator::initTimer(); //start timer for measuring frequency

    analogWriteResolution(12); //change DAC to 12-bit resolution

    DAC0.begin();
    //TODO: make array in init values
    lfoRateDac.write(0.5);
    vcfCutDac.write(0.5);

    //Digipot--------------------------------------------------------
    digiPot1.begin();
    pwmPot.write(1); //max
    lfoToPitchPot.write(0); //min
    envToVcfPot.write(.5); //Env to VCF

    digiPot2.begin();
    resonancePot.write(.1);
    vcfDrivePot.write(0);

    adsr.begin(); //starts PMW timers
    voice.osc.calibrate(); //call CV calibration routine

//    u8g2.begin(); //start OLED
//    u8g2.setPowerSave(true); //turn display off

    //NVIC_DisableIRQ(AGT0_INT_IRQn);

    //log a series of measurements for HF trim adjustment
//    osc.start();
//    for (uint16_t note = 24; note <= 108; note++) {
//        osc.setNote(note);
//        delay(100);
//        float mes = osc.measureFreq();
//        Serial.println(mes, 3);
//    }
//    osc.stop();

    //set up GPT4 timer to interrupt at 4kHz. this is the tick for the modulators
    tickTimer.begin(TIMER_MODE_PERIODIC, GPT_TIMER, 4, 4000.0f, 0.0f, tickClock);
    tickTimer.setup_overflow_irq();
    tickTimer.open();
    tickTimer.start();

    //MIDI callbacks
    midi.noteOn = noteOnHandler;
    midi.noteOff = noteOffHandler;
    midi.controlChange = midiCcHandler;
    midi.pitchBend = [](int16_t bend) { voice.setPitchBend(bend); };

    //set touchstrip callbacks--------------------------------------
    constexpr float TOUCH_SCALE = 80.0;
    constexpr float TOUCH_OFFSET = 24;
    touchStrip.pressedCB = [](float reading){
        float scaledNote = reading * TOUCH_SCALE + TOUCH_OFFSET;
        voice.noteOn(scaledNote, 100);
    };

    touchStrip.updatedCB = [](float reading){
        float scaledNote = reading * TOUCH_SCALE + TOUCH_OFFSET;
        voice.osc.setNote(scaledNote); //play note
    };

    touchStrip.releasedCB = [](float reading){
        float scaledNote = reading * TOUCH_SCALE + TOUCH_OFFSET;
        voice.noteOff(scaledNote, 100);
        voice.osc.setNote(scaledNote); //play note
    };
}

void loop() {
    while (Serial1.available())
        midi.handleByte(Serial1.read());

    //update OLED screen
//    char buffer[30]; //text buffer
//    u8g2.clearBuffer();
//
//    u8g2.setFont(u8g2_font_6x13_tr);
//    sprintf(buffer, "Byte: %d", test);
//    u8g2.drawStr(0, 10, buffer);
//    u8g2.sendBuffer();

    if (stepFlag) { //4kHz
        stepFlag = false;

        voice.update(); //glide, keytrack, etc

        //TODO: remove. testing VCA with LFO output
        lfoRateDac.write(1 - voice.vcaADSR.output);

        touchStrip.poll();
    }
}

//find item in array
uint8_t *find(uint8_t *first, uint8_t *last, uint8_t value) {
    for (; first != last; first++) if (*first == value) return first;

    return last;
}

void noteOnHandler(uint8_t note, uint8_t vel) {
    //if (note < osc.LOW_NOTE || note > (osc.LOW_NOTE + 48)) return; //invalid note

    //if (note > osc.LOW_NOTE) note -= osc.LOW_NOTE;

    //put key into array if it's not there already
    uint8_t *arrEnd = pressedKeys + keyCount + 1;
    if (find(pressedKeys, arrEnd, note) == arrEnd)
        pressedKeys[keyCount++] = note;

    voice.noteOn(note, vel);
}

void noteOffHandler(uint8_t note, uint8_t vel) {
    //if (note < osc.LOW_NOTE || note > (osc.LOW_NOTE + 48)) return; //invalid note

    //note -= osc.LOW_NOTE;

    //remove key from array if it's there
    uint8_t *arrEnd = pressedKeys + keyCount + 1;
    uint8_t *keyPtr = find(pressedKeys, arrEnd, note);
    if (keyPtr != arrEnd) { //found
        for (; keyPtr < arrEnd; keyPtr++) //fill in hole
            *keyPtr = *(keyPtr + 1);

        keyCount--;
    }

    //TODO: add note priority options (high, low, last)
    if (keyCount > 0) //fallback to last note
        voice.noteOn(pressedKeys[keyCount - 1], vel);
    else
        voice.noteOff(pressedKeys[keyCount - 1], vel);
}

//------------------------------------
struct CCbind { //binds MIDI CC number to Param
    uint8_t ccNumber;
    Param& param;
};

constexpr uint8_t PARAM_COUNT = 27;
const CCbind ccs[PARAM_COUNT] = {
    //{ 5, voice.glideTime },
    { 5, voice.osc.pwmADSR.attack },
    { 6, voice.osc.pwmADSR.decay },
    { 7, voice.osc.pwmADSR.sustain },
    { 8, voice.osc.pwmADSR.release },
    { 9,  voice.vcaADSR.attack },
    { 10, voice.vcaADSR.decay },
    { 11, voice.vcaADSR.sustain },
    { 12, voice.vcaADSR.release },
    { 13, voice.vcf.drive },
    { 14, voice.vcf.cutoff },
    { 15, voice.osc.waveform },
    { 16, voice.osc.pwm },
    { 17, voice.osc.pwmLFO.rate },
    { 18, voice.osc.pwmLFO.waveform },
    { 19, voice.osc.pwmLFO.depth },
    { 20, voice.osc.pwmLFO.reset },
    { 21, voice.osc.pwmDA.delay },
    { 22, voice.osc.pwmDA.attack },
    { 23, voice.osc.pwmADSR.sampHoldClock.rate },
    { 24, voice.vcf.keyTracking },
    { 25, voice.vcfAccAmt },

    { 29, voice.env.attack },
    { 30, voice.env.decay },
    { 31, voice.env.sustain },
    { 32, voice.env.release },

    { 71, voice.vcf.resonance },
    { 72, voice.lfo.rate },
};

void midiCcHandler(uint8_t cc, uint8_t val) {
    //find param with this CC number
    CCbind *result = (CCbind *) bsearch(&cc, ccs, PARAM_COUNT, sizeof(CCbind),
        [](const void *key, const void *ptr) -> int {
            int8_t ccNum = *(const uint8_t *) key;
            const CCbind *ccBind = (const CCbind *) ptr;

            return ccNum - (int8_t)ccBind->ccNumber;
        }
    );

    if (result) {
        result->param.set(val); //store raw value

        //TODO: buffer these strings and rate limit when screen/serial updates
        char buffer[Param::VAL_BUFFER_LEN] = {};
        Serial.println(result->param.fullName(buffer, sizeof(buffer)));
        Serial.println(result->param.toString(buffer, sizeof(buffer)));

        return;
    }

    switch (cc) {
        case 1: //Mod wheel
            lfoToPitchPot.write(val / 127.0);
            break;
        case 73: //Env depth
            envToVcfPot.write(val / 127.0); //Env to VCF
            break;
    }
}

//expo approximations

//            const float q = 64.0;
//            const float expoApprox = (!val) ? 100 : (normVal / (q + (1 - q) * normVal)) * 100;

//power law
//            const float w = 1 - (1/50); //0.996;
//            const float q = 10.7; //8.8;
//            float expoApprox = normVal - (w * (normVal - pow(normVal, q)));
//-----------------------------------------------------------------------------------