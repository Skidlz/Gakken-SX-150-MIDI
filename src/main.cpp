#include <Arduino.h>
//#include <U8g2lib.h>
#include "FspTimer.h"
#include "midi.h"
#include "oscillator.h"
#include "voice.h"
#include "MCP4251.h" //Digipot
#include "pwm_zen.h" //customized PWM library
#include "BD79702.h" //DAC
#include "modulator.h"
#include "hw_adsr.h"
#include "analogInputs.h"

BD79702 DAC0(8);
BD79702::CHAN SUSTAIN = BD79702::AO1, LFO_RATE = BD79702::AO2, VCF_CUT = BD79702::AO3,
    TEST_OUT = BD79702::AO4; //output to experiment with

FspTimer tickTimer; //GPT4

SW_LFO pwmLFO;
SW_DA pwmDA; //delay attack envelope
SW_ADSR pwmADSR;
SW_ADSR accentADSR;
SW_ADSR vcaADSR;
//hardware ADSR, use DAC for sustain
HW_adsr adsr(D3, D5, D4, &DAC0, SUSTAIN);
touchStrip touchStrip;

bool stepFlag = false; //modulation tick

void tickClock(timer_callback_args_t *args) { stepFlag = true; }

//128x32 I2C OLED on pins A4 (data) & A5 (clock)
//U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

//const char *const noteNames[12] = { "A", "A#/Bb", "B", "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab" };

Voice voice; //voice contains Osc
MIDI midi;
MCP4251 digiPot1; //50k
MCP4251::Pot LFO_DEPTH = MCP4251::POT0, PWM_POT = MCP4251::POT2; //ENV_PITCH
MCP4251 digiPot2(9); //10k
MCP4251::Pot ENV_AMT = MCP4251::POT0, RESONANCE = MCP4251::POT1, unused = MCP4251::POT2, VCF_DRIVE = MCP4251::POT3;

//MIDI callbacks-----------------------------------------------------
void noteOnHandler(uint8_t note, uint8_t vel);
void noteOffHandler(uint8_t note, uint8_t vel);
void pitchBendHandler(int16_t bend);
void midiCcHandler(uint8_t cc, uint8_t val);

uint8_t pressedKeys[127]; //hold all currently pressed notes
//allows us to fall back to previously pressed note after release
uint8_t keyCount = 0;
bool glideLegato = true; //only slide when pressing more than one note
float glideRate = 1; //used to turn off glide at minimum
bool invertSaw = false; //inverts saw relative to pulse 1 & 2
Oscillator::Waveform waveform = Oscillator::SAW;

float vcfOffset = .5;
float keyTracking = .3;

void updateVCF() {
    float newVCFcut = vcfOffset + ((keyTracking == 0) ? 0 : ((voice.osc.currentNote / 127.0) * keyTracking));
    newVCFcut += accentADSR.output * 0.25; //TODO: adjust accent depth
    if (newVCFcut > 1) newVCFcut = 1; //cap at max
    DAC0.setDAC(VCF_CUT, newVCFcut * 255);
}

void setup() {
    Serial1.begin(31250); //MIDI baud
    Serial.begin(115200); //debug port
    //delay(1000); //let serial connect

    //Terminal needs RTS/CTS flow to work!
    //while (!Serial) {} //wait for debug serial port to connect

    midi.noteOn = noteOnHandler; //callbacks
    midi.noteOff = noteOffHandler;
    midi.pitchBend = pitchBendHandler;
    midi.controlChange = midiCcHandler;

    Oscillator::initTimer(); //start timer for measuring frequency

    //u8g2.begin(); //start OLED

    pinMode(LED_BUILTIN, OUTPUT);
    //digitalWrite(LED_BUILTIN, HIGH);

    analogWriteResolution(12); //change DAC to 12-bit resolution

    DAC0.begin();
    DAC0.setDAC(LFO_RATE, 127);
    DAC0.setDAC(VCF_CUT, 127);

    adsr.begin(); //starts PMW timers

    //Digipot--------------------------------------------------------
    digiPot1.begin();
    digiPot1.setWiper(PWM_POT, 127);
    digiPot1.setWiper(LFO_DEPTH, 0);
    digiPot1.setWiper(ENV_AMT, 255); //Env to VCF

    digiPot2.begin();
    digiPot2.setWiper(ENV_AMT, 127);
    digiPot2.setWiper(RESONANCE, 127);
    //digiPot2.setWiper(?, 127);
    digiPot2.setWiper(VCF_DRIVE, 127);

    midiCcHandler(1, 0); //LFO depth

    //startup message
//    u8g2.clearBuffer();
//    u8g2.setFont(u8g2_font_6x13_tr);
//    u8g2.setPowerSave(true); //turn display off

    voice.osc.calibrate(); //call CV calibration routine
    //NVIC_DisableIRQ(AGT0_INT_IRQn);

    //log a series of measurements
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

    Serial.println("Setup done");

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

        //only glide when key(s) held
        if (keyCount) voice.updateGlide();

        //update modulation. send values to digiPots
        pwmLFO.step();
        pwmDA.step();
        //scale and offset LFO
        uint8_t pwmValue = 255 - (((pwmLFO.output * pwmDA.output) + 1) * (127 / 2));
        //pwmADSR.step();
        //uint8_t pwmValue = 255 - (pwmADSR.output * 127);
        digiPot1.setWiper(PWM_POT, pwmValue); //PWM & super saw adjust

        accentADSR.step();
        updateVCF(); //update keytracking

        vcaADSR.step();
        //TODO: remove. testing VCA with LFO output
        DAC0.setDAC(LFO_RATE, ((1 - vcaADSR.output) * 255));

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

    //turn glide on if more than one note is pressed
    if (keyCount > 1 && glideLegato && glideRate > 0.0) voice.glideOn = true;

    voice.noteOn(note, vel);
    pwmADSR.gateOn(); //TODO: add legato trigger option
    vcaADSR.gateOn();
    pwmDA.gateOn();

    if (vel > 100) accentADSR.gateOn();

    updateVCF(); //keytracking

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
        accentADSR.gateOff();
        if (glideLegato) voice.glideOn = false;
    }
}

void pitchBendHandler(int16_t bend) {
    voice.setPitchBend(bend);
}

void midiCcHandler(uint8_t cc, uint8_t val) {
    //need CC to change glide type and glide legato mode
    switch (cc) {
        case 1: { //Mod wheel
            float normVal = val / 127.0;

            //LFO depth
            if (normVal == 0.0) { //disconnect POT leg at minimum value
                digiPot1.disconnectLeg(LFO_DEPTH, MCP4251::B);
            } else {
                //reconnect leg
                if (digiPot1.getLegStatus(LFO_DEPTH, MCP4251::B))
                    digiPot1.connectLeg(LFO_DEPTH, MCP4251::B);

                //apply S-curve to compensate for imperfect log approximation circuit
                constexpr float x = 0.51; //sets the middle of the curve on x-axis
                constexpr float k = 3.8; //sets the curve severity
                //precomputed offset and denominator
                constexpr float offset = 0.125867742017; // = 1 / (1 + exp(-k * (0 - x)));
                constexpr float scale = 255 / (0.865529894061 - offset); // = 1 / (1 + exp(-k * (1 - x))) - offset;

                //apply s-curve formula
                float sCurve = 1 / (1 + exp(-k * (normVal - x)));
                sCurve = (sCurve - offset) * scale;
                digiPot1.setWiper(LFO_DEPTH, (255 - sCurve));
            }

            break;
        }
        case 5: //Portamento Time
            //voice.glideRate = val / 127.0;
//            glideRate = val / 127.0;
//            voice.glideOn = !!val; //force portamento on. could let another CC control it?
//            voice.setGlideRate(val / 127.0);

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
            digiPot1.setWiper(PWM_POT, 255 - val ); //PWM & super saw adjust
            break;

        case 14: //VCF Cut
            vcfOffset = val / 127.0;
            updateVCF();
            break;

        case 15: //Waveform select
            //use the upper 4 bits to select from 16 waveforms
            waveform = static_cast<Oscillator::Waveform>((val * 20) / 127);
            voice.osc.setWaveform(waveform);
            break;

        case 16: //VCF Drive
            digiPot2.setWiper(VCF_DRIVE, 255 - (val << 1));
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
            pwmLFO.offset = (val * 2 / 127.0) - 1;
            break;

        case 21:
            pwmDA.setRate(SW_DA::DELAY, val / 127.0);
            break;

        case 22:
            pwmDA.setRate(SW_DA::ATTACK, val / 127.0);
            break;

        case 23:

            break;

        case 24: { //VCF keytracking
            keyTracking = val / 127.0;
            updateVCF();
            break;
        }

        case 25: {
            //invertSaw = (val >= 64);
            //voice.osc.setWaveform(waveform, invertSaw);
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
            digiPot2.setWiper(RESONANCE, 255 - (val << 1)); //VCF resonance
            break;

        case 72: //LFO rate
            //DAC0.setDAC(LFO_RATE, 255 - (val << 1));
            break;

        case 73: //Env depth
            digiPot2.setWiper(ENV_AMT, 255 - (val << 1)); //Env to VCF
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

//// ADC register access
//#define ADC0_BASE 0x4005C000
//
//// Configure ADC for single conversion
//void setupADC(int channel) {
//  // Enable ADC module
//  R_MSTP->MSTPCRC_b.MSTPC31 = 0;  // Enable ADC0
//
//  // Configure channel (0-7 for analog pins A0-A7)
//  R_ADC0->ADANSA_b[0].ANSA0 = 1;  // Select channel
//
//  // Set 12-bit resolution
//  R_ADC0->ADCER_b.ADPRC = 0;  // 12-bit
//
//  // Configure for software trigger, single scan
//  R_ADC0->ADCSR_b.ADCS = 0;   // Single scan mode
//  R_ADC0->ADCSR_b.TRGE = 0;   // Software trigger
//
//  // Optional: Enable conversion complete interrupt
//  R_ADC0->ADCSR_b.ADIE = 1;   // Enable interrupt
//  NVIC_EnableIRQ(ADC0_SCAN_END_IRQn);
//}
//
//// Start conversion (non-blocking)
//void startADC() {
//  R_ADC0->ADCSR_b.ADST = 1;  // Start conversion
//}
//
//// Check if conversion is complete
//bool isADCComplete() {
//  return R_ADC0->ADCSR_b.ADST == 0;  // ADST clears when done
//}
//
//// Read result
//uint16_t readADC(int channel) {
//  return R_ADC0->ADDR[channel];  // Read result register
//}
//
//// Optional: ISR for conversion complete
//volatile bool adcReady = false;
//volatile uint16_t adcValue = 0;
//
//extern "C" void adc0_scan_end_isr(void) {
//  adcValue = R_ADC0->ADDR[0];  // Read channel 0
//  adcReady = true;
//}