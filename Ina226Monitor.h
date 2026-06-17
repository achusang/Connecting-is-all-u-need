#pragma once

#include <Arduino.h>
#include <Wire.h>

class Ina226Monitor {
public:
  struct Data {
    bool ok = false;
    float busV = 0.0f;
    float shuntmV = 0.0f;
    float currentA = 0.0f;
    float powerW = 0.0f;
  };

  bool begin(TwoWire& wire, uint8_t addr, float shuntOhm) {
    _wire = &wire;
    _addr = addr;
    _shuntOhm = shuntOhm;

    writeReg(0x00, 0x8000);   // reset
    delay(10);
    writeReg(0x00, 0x4127);   // 连续测量 shunt + bus
    delay(10);

    uint16_t cfg = 0;
    return readReg(0x00, cfg);
  }

  Data read() {
    Data d;
    uint16_t regShunt = 0, regBus = 0;

    bool ok = readReg(0x01, regShunt) && readReg(0x02, regBus);
    if (!ok) return d;

    int16_t shuntSigned = (int16_t)regShunt;
    float shuntV = shuntSigned * 2.5e-6f;
    float busV   = regBus * 0.00125f;

    d.ok = true;
    d.busV = busV;
    d.shuntmV = shuntV * 1000.0f;
    d.currentA = (_shuntOhm > 0) ? (shuntV / _shuntOhm) : 0.0f;
    d.powerW = d.busV * d.currentA;
    return d;
  }

private:
  TwoWire* _wire = nullptr;
  uint8_t _addr = 0x40;
  float _shuntOhm = 0.1f;

  void writeReg(uint8_t reg, uint16_t val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write((val >> 8) & 0xFF);
    _wire->write(val & 0xFF);
    _wire->endTransmission();
  }

  bool readReg(uint8_t reg, uint16_t &val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return false;
    if (_wire->requestFrom((int)_addr, 2) != 2) return false;
    val = ((uint16_t)_wire->read() << 8) | _wire->read();
    return true;
  }
};
