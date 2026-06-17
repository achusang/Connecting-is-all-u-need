#pragma once

#include <Arduino.h>
#include <math.h>

class TempPidController {
public:
  void setTunings(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
  }

  void setOutputLimits(float outMin, float outMax) {
    _outMin = outMin;
    _outMax = outMax;
    if (_integral > _outMax) _integral = _outMax;
    if (_integral < _outMin) _integral = _outMin;
  }

  void setDeadband(float deadbandC) {
    _deadbandC = deadbandC;
  }

  void reset() {
    _integral = 0.0f;
    _prevInput = NAN;
    _prevOutput = 0.0f;
  }

  float kp() const { return _kp; }
  float ki() const { return _ki; }
  float kd() const { return _kd; }
  float deadband() const { return _deadbandC; }
  float integralTerm() const { return _integral; }
  float lastOutput() const { return _prevOutput; }

  float compute(float setpoint, float input, float dtSec) {
    if (dtSec <= 0.0f || isnan(setpoint) || isnan(input)) {
      return _prevOutput;
    }

    float error = setpoint - input;
    if (fabsf(error) < _deadbandC) {
      error = 0.0f;
    }

    // 微分对测量值，避免 setpoint 跳变时微分冲击
    float dInput = 0.0f;
    if (!isnan(_prevInput)) {
      dInput = (input - _prevInput) / dtSec;
    }

    float pTerm = _kp * error;
    float dTerm = -_kd * dInput;

    // 先预测未积分输出，做简单 anti-windup
    float preOutput = pTerm + _integral + dTerm;

    bool allowIntegrate = true;
    if (preOutput >= _outMax && error > 0) allowIntegrate = false;
    if (preOutput <= _outMin && error < 0) allowIntegrate = false;

    if (allowIntegrate) {
      _integral += _ki * error * dtSec;
      if (_integral > _outMax) _integral = _outMax;
      if (_integral < _outMin) _integral = _outMin;
    }

    float output = pTerm + _integral + dTerm;

    if (output > _outMax) output = _outMax;
    if (output < _outMin) output = _outMin;

    _prevInput = input;
    _prevOutput = output;
    return output;
  }

private:
  float _kp = 20.0f;
  float _ki = 0.15f;
  float _kd = 0.0f;
  float _deadbandC = 0.10f;

  float _outMin = 0.0f;
  float _outMax = 100.0f;

  float _integral = 0.0f;
  float _prevInput = NAN;
  float _prevOutput = 0.0f;
};
