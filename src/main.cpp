#include <Arduino.h>
//#include <U8g2lib.h>
#include "FspTimer.h"
#include "midi.h"
#include "hw_vco.h"
#include "hw_adsr.h"
#include "voice.h"
#include "MCP4251.h" //Digipot
#include "BD79702.h" //DAC
#include "modulator.h" //Software modulators
#include "analogInputs.h"
#include <functional>

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

//hardware ADSR, use DAC for sustain
HW_adsr adsr(D3, D5, D4, sustainDac);

touchStrip touchStrip;

SW_LFO pwmLFO;
SW_DA pwmDA; //delay attack envelope
SW_ADSR pwmADSR;
SW_ADSR vcaADSR;

FspTimer tickTimer; //GPT4
bool stepFlag = false; //modulation tick

void tickClock(timer_callback_args_t *args) { stepFlag = true; }

//128x32 I2C OLED on pins A4 (data) & A5 (clock)
//U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
//const char *const noteNames[12] = { "A", "A#/Bb", "B", "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab" };

Voice voice(vcfCutDac, resonancePot); //voice contains Osc
MIDI midi;

//MIDI callbacks-----------------------------------------------------
void noteOnHandler(uint8_t note, uint8_t vel);
void noteOffHandler(uint8_t note, uint8_t vel);
void midiCcHandler(uint8_t cc, uint8_t val);

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

//    u8g2.begin(); //start OLED
//    u8g2.setPowerSave(true); //turn display off

    voice.osc.calibrate(); //call CV calibration routine
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

    //set touchstrips callbacks
    touchStrip.pressedCB = [](float reading){
        pwmADSR.gateOn();
        vcaADSR.gateOn();
        voice.osc.start(); //gate pin
        voice.osc.setNote(reading * 80 + 24); //play note
    };

    touchStrip.updatedCB = [](float reading){
        voice.osc.setNote(reading * 80 + 24); //play note
    };

    touchStrip.releasedCB = [](float reading){
        pwmADSR.gateOff();
        vcaADSR.gateOff();
        voice.osc.stop(); //gate pin

        voice.osc.setNote(reading * 80 + 24); //play note
    };

    //voice.setGlideRate(.25);
}

void loop() {
    char buffer[30]; //text buffer

    while (Serial1.available()) {
        char test = Serial1.read();

        //update OLED screen
//        u8g2.clearBuffer();
//
//        u8g2.setFont(u8g2_font_6x13_tr);
//        sprintf(buffer, "Byte: %d", test);
//        u8g2.drawStr(0, 10, buffer);
//        u8g2.sendBuffer();

        midi.handleByte(test);
    }

    //---------------------------------------------------------

    if (stepFlag) { //4kHz
        stepFlag = false;

        //update modulation. send values to digiPots
        pwmLFO.step();
        pwmDA.step();
        //pwmADSR.step();
        //scale and offset LFO
        float pwmValue = pwmLFO.offset + (pwmLFO.output * pwmDA.output * pwmLFO.scale) / 2;
        pwmPot.write(pwmValue / 2); //max

        voice.update(); //glide, keytrack, etc

        vcaADSR.step();
        //TODO: remove. testing VCA with LFO output
        lfoRateDac.write(1 - vcaADSR.output);

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
    pwmADSR.gateOn(); //TODO: add legato trigger option
    vcaADSR.gateOn();
    pwmDA.gateOn();

    //extra envelope for accent
    //digiPot.setWiper(ENV_AMT, (vel > 100) ? 255: 220); //Env to VCF
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
    else {
        voice.noteOff(pressedKeys[keyCount - 1], vel);
        pwmADSR.gateOff();
        vcaADSR.gateOff();
    }
}

void midiCcHandler(uint8_t cc, uint8_t val) {
    switch (cc) {
        case 1: //Mod wheel
            lfoToPitchPot.write(val / 127.0);
            break;

        case 5: //Portamento Time
            //voice.setGlideRate(val / 127.0);

            pwmADSR.setRate(SW_ADSR::ATTACK, val / 127.0);
            break;

        case 6:
            pwmADSR.setRate(SW_ADSR::DECAY, val / 127.0);
            break;

        case 7:
            pwmADSR.setRate(SW_ADSR::SUSTAIN, val / 127.0);
            break;

        case 8:
            pwmADSR.setRate(SW_ADSR::RELEASE, val / 127.0);
            break;

        case 9: //attack
//            adsr.setRate(HW_adsr::ATTACK, val / 127.0);
            vcaADSR.setRate(SW_ADSR::ATTACK, val / 127.0);
            break;

        case 10: //decay Time
//            adsr.setRate(HW_adsr::DECAY, val / 127.0);
            vcaADSR.setRate(SW_ADSR::DECAY, val / 127.0);
            break;

        case 11: //sustain level
//            adsr.setSustain(val/127.0);
            vcaADSR.setSustain(val / 127.0);
            break;

        case 12: //release time
//            adsr.setRate(HW_adsr::RELEASE, val / 127.0);
            vcaADSR.setRate(SW_ADSR::RELEASE, val / 127.0);
            break;

        case 13: //PWM & super saw adjust
            //only use half of pot
//            digiPot1.setWiper(PWM_POT, 255 - val ); //PWM & super saw adjust
            pwmPot.write(1 - (val / 127.0));
            break;

        case 14: //VCF Cut
            voice.vcf.cut = val / 127.0;
            voice.vcf.updateCut(voice.currentGlideNote, voice.accentADSR.output * 0.25);
            break;

        case 15: //Waveform select
            //use the upper 4 bits to select from 16 waveforms
            voice.osc.setWaveform(static_cast<Oscillator::Waveform>((val * 20) / 127));
            break;

        case 16: //VCF Drive
            vcfDrivePot.write(val / 127.0);
            break;

        case 17: //Soft LFO for PWM
            pwmLFO.setRate(val / 127.0);
            break;

        case 18: //Soft LFO waveform
            pwmLFO.setWaveform(static_cast<SW_LFO::Waveform>((val >> 5) & 0b11));
            break;

        case 19: //Soft LFO depth
            pwmLFO.scale = val / 127.0;
            break;

        case 20: //PWM offset
            pwmLFO.offset = val / 127.0; //(val * 2 / 127.0) - 1;
            break;

        case 21:
            pwmDA.setRate(SW_DA::DELAY, val / 127.0);
            break;

        case 22:
            pwmDA.setRate(SW_DA::ATTACK, val / 127.0);
            break;

        case 23:

            break;

        case 24: { //VCF key tracking
            voice.vcf.keyTracking = val / 127.0;
            voice.vcf.updateCut(voice.currentGlideNote, voice.accentADSR.output * 0.25);
            break;
        }

        case 25: {
            //invertSaw = (val >= 64);
            pwmADSR.sampHoldClock.setRate(val / 127.0);
            pwmADSR.sampleHold = !!(val);
            break;
        }

        case 29: //attack
            adsr.setRate(HW_adsr::ATTACK, val / 127.0);
            break;

        case 30: //decay Time
            adsr.setRate(HW_adsr::DECAY, val / 127.0);
            break;

        case 31: //sustain level
            adsr.setSustain(val/127.0);
            break;

        case 32: //release time
            adsr.setRate(HW_adsr::RELEASE, val / 127.0);
            break;

        case 71: //Resonance
            voice.vcf.updateResonance(val / 127.0);
            break;

        case 72: //LFO rate
            lfoRateDac.write(1 - (val / 127.0));
            break;

        case 73: //Env depth
            envToVcfPot.write(val / 127.0); //Env to VCF
            break;

        default:
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