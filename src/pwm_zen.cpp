#include "pwm_zen.h"

FspTimer *PWM::_timers[8] = { nullptr };

void PWM::begin(uint32_t freq) {
    auto pin_cgf = getPinCfgs(_pin, PIN_CFG_REQ_PWM);

    //set up timer--------------------------------------------
    uint8_t GPT = GET_CHANNEL(pin_cgf[0]);
    const uint16_t periodCycles = F_CPU / freq; //cpu cycles per period

    if (_timers[GPT] != nullptr) { //timer already exists
        _timer = _timers[GPT];

        if (_timer->is_opened()) {
            _timer->stop();
            _timer->close();
        }

        _timer->set_period(periodCycles); //update freq
    } else {
        _timers[GPT] = new FspTimer(); //put timer in array

        _timer = _timers[GPT]; //set up new timer
        _timer->begin(TIMER_MODE_PWM, GPT_TIMER, GPT,
                      periodCycles, periodCycles / 2, TIMER_SOURCE_DIV_1);
        //_timer->add_pwm_extended_cfg();
    }

    //set up pin----------------------------------------------------
    _pwmChan = IS_PWM_ON_A(pin_cgf[0]) ? CHANNEL_A : CHANNEL_B;
    _timer->enable_pwm_channel(_pwmChan); //enable correct output channel A/B

    setDutyCycle(.5); //init to 50%

    //configure pin for GPT peripheral
    R_IOPORT_PinCfg(&g_ioport_ctrl, g_pin_cfg[_pin].pin, IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_GPT1);

    _timer->open();
    _timer->start();
}

void PWM::setDutyCycle(float duty) {
    float period = (float) _timer->get_period_raw();
    float pulse = period * duty;
    _timer->set_duty_cycle((int) pulse, _pwmChan);
}