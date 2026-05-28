#include <Arduino.h>
#include "analogInputs.h"
#include "analog.h"

extern ADC_Container adc;

touchStrip::touchStrip() {
    begin();
}

void touchStrip::begin() { //adapted from analogRead()
    int32_t adc_idx = digitalPinToAnalogPin(A6);
    auto cfg_adc = getPinCfgs(adc_idx, PIN_CFG_REQ_ADC);
    adc_cfg = cfg_adc[0];
    //can't call get_ADC_container_ptr
    pinPeripheral(digitalPinToBspPin(adc_idx), (uint32_t) IOPORT_CFG_ANALOG_ENABLE);
    R_ADC0->ADANSA[0] |= (1 << GET_CHANNEL(adc_cfg)); //enable

    R_ADC0->ADCSR_b.ADST = 1; //start ADC read
}

void touchStrip::poll() {
    if (R_ADC0->ADCSR_b.ADST == 1) return; //wait for ADC to be done

    int16_t newReading = R_ADC0->ADDR[GET_CHANNEL(adc_cfg)];
    bool pressed = (newReading > LOW_THRESH);

    if (gateTimer) gateTimer--;

    //don't retrigger if already triggered
    if (pressed && prevReading < LOW_THRESH && !gate) {
        gate = true;
        gateTimer = MIN_GATE_TICKS; //reset timer to force minimum gate length

        smoothedReading = scaleAndOffset(newReading);

        //wipe out old history for previous note
        std::fill_n(history, HIST_COUNT, smoothedReading);
        
        if (pressedCB) pressedCB(smoothedReading); //callback
    } else if (!pressed && gate && !gateTimer) { //wait for timeout before releasing
        gate = false;

        //use historic value because readings deviate as releasing
        smoothedReading = history[nextIndex(writeIndex)]; //oldest reading

        if (releasedCB) releasedCB(smoothedReading); //callback
    }
    
    //update value while pressed
    if (pressed && gateTimer != MIN_GATE_TICKS) { //don't need to update *on* press
        float scaledReading = scaleAndOffset(newReading);

        //smooth out hf jitter, but allow large jumps
        int16_t delta = abs(newReading - prevReading);
        smoothedReading = (abs(delta) > SMOOTH_MAX_DELTA) ? scaledReading
                : smoothedReading + SMOOTH_ALPHA * (float)(scaledReading - smoothedReading);

        if (updatedCB) updatedCB(smoothedReading); //callback

        //store reading history for release
        writeIndex = nextIndex(writeIndex);
        history[writeIndex] = smoothedReading;
    }

    prevReading = newReading;
    output = smoothedReading;
    R_ADC0->ADCSR_b.ADST = 1; //start another conversion
}
