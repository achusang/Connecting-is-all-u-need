#pragma once

#include <Arduino.h>
#include "Config.h"

class PwmController {
public:
  bool begin(int pin, uint32_t freq, uint8_t resolution) {
    _pin = pin;
    _freq = freq;
    _resolution = resolution;
    _maxDuty = (1 << resolution) - 1;

    bool ok = ledcAttach(_pin, _freq, _resolution);
    if (!ok) return false;

    setDuty(0);
    return true;
  }

  void setDuty(int duty) {
    if (duty < 0) duty = 0;
    if (duty > _maxDuty) duty = _maxDuty;
    _duty = duty;
    ledcWrite(_pin, _duty);
  }

  void setPercent(float percent) {
    if (percent < 0) percent = 0;
    if (percent > PWM_PERCENT_LIMIT) percent = PWM_PERCENT_LIMIT;
    int duty = (int)((percent / 100.0f) * _maxDuty + 0.5f);
    setDuty(duty);
  }

  int duty() const { return _duty; }
  int maxDuty() const { return _maxDuty; }

  float percent() const {
    if (_maxDuty == 0) return 0.0f;
    return 100.0f * _duty / _maxDuty;
  }

private:
  int _pin = -1;
  uint32_t _freq = 0;
  uint8_t _resolution = 8;
  int _maxDuty = 255;
  int _duty = 0;
};
