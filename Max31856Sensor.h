#pragma once

#include <Arduino.h>
#include <Adafruit_MAX31856.h>
#include <math.h>

class Max31856Sensor {
public:
  struct Data {
    bool ok = false;
    float tcTempC = NAN;
    float cjTempC = NAN;
    uint8_t fault = 0;
  };

  Max31856Sensor(
    int csPin,
    int mosiPin,
    int misoPin,
    int sckPin,
    max31856_thermocoupletype_t tcType
  )
  : _thermo(csPin, mosiPin, misoPin, sckPin), _type(tcType) {}

  bool begin() {
    if (!_thermo.begin()) return false;
    _thermo.setThermocoupleType(_type);
    return true;
  }

  Data read() {
    Data d;
    d.fault = _thermo.readFault();
    d.cjTempC = _thermo.readCJTemperature();
    d.tcTempC = _thermo.readThermocoupleTemperature();
    d.ok = true;
    return d;
  }

  void printFault(Stream& s, uint8_t f) {
    if (f == 0) {
      s.print("NONE");
      return;
    }

    bool first = true;
    auto add = [&](const char* name) {
      if (!first) s.print(",");
      s.print(name);
      first = false;
    };

    if (f & MAX31856_FAULT_CJRANGE) add("CJRANGE");
    if (f & MAX31856_FAULT_TCRANGE) add("TCRANGE");
    if (f & MAX31856_FAULT_CJHIGH)  add("CJHIGH");
    if (f & MAX31856_FAULT_CJLOW)   add("CJLOW");
    if (f & MAX31856_FAULT_TCHIGH)  add("TCHIGH");
    if (f & MAX31856_FAULT_TCLOW)   add("TCLOW");
    if (f & MAX31856_FAULT_OVUV)    add("OVUV");
    if (f & MAX31856_FAULT_OPEN)    add("OPEN");
  }

private:
  Adafruit_MAX31856 _thermo;
  max31856_thermocoupletype_t _type;
};
