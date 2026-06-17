#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

class SdCsvLogger {
public:
  bool begin(int sckPin, int misoPin, int mosiPin, int csPin, const char* path) {
    _csPin = csPin;
    _path = path;

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    _spi = new SPIClass(FSPI);
    _spi->begin(sckPin, misoPin, mosiPin, csPin);

    if (!SD.begin(csPin, *_spi)) {
      _ready = false;
      return false;
    }

    _ready = true;

    if (!SD.exists(_path.c_str())) {
      File f = SD.open(_path.c_str(), FILE_WRITE);
      if (!f) {
        _ready = false;
        return false;
      }
      f.println("datetime,time_ms,mode,pwm_duty,pwm_pct,bus_v,current_a,power_w,t_amb_c,t_sample_c,setpoint_c,error_c,pid_out_pct,fault1,fault2");
      f.flush();
      f.close();
    }

    return true;
  }

  bool ready() const { return _ready; }

  bool appendLine(const String& line) {
    if (!_ready) return false;

    File f = SD.open(_path.c_str(), FILE_APPEND);
    if (!f) return false;

    f.println(line);
    f.flush();
    f.close();
    return true;
  }

private:
  SPIClass* _spi = nullptr;
  bool _ready = false;
  int _csPin = -1;
  String _path = "/log.csv";
};
