#pragma once

#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

class Ds1302Clock {
public:
  Ds1302Clock(int datPin, int clkPin, int rstPin)
  : _wire(datPin, clkPin, rstPin), _rtc(_wire) {}

  bool begin() {
    _rtc.Begin();

    if (_rtc.GetIsWriteProtected()) {
      _rtc.SetIsWriteProtected(false);
    }

    if (!_rtc.GetIsRunning()) {
      _rtc.SetIsRunning(true);
    }

    _valid = _rtc.IsDateTimeValid();

    // 如果 RTC 完全没时间，先给一个编译时间兜底。
    // 第一次使用时，建议用串口命令 settime YYYY-MM-DD HH:MM:SS 设置真实时间。
    if (!_valid) {
      RtcDateTime compiled(__DATE__, __TIME__);
      _rtc.SetDateTime(compiled);
      _valid = _rtc.IsDateTimeValid();
    }

    return true;
  }

  bool isValid() const {
    return _valid;
  }

  RtcDateTime now() {
    RtcDateTime dt = _rtc.GetDateTime();
    _valid = _rtc.IsDateTimeValid();
    return dt;
  }

  bool setDateTime(int year, int month, int day, int hour, int minute, int second) {
    if (year < 2000 || year > 2099) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if (hour < 0 || hour > 23) return false;
    if (minute < 0 || minute > 59) return false;
    if (second < 0 || second > 59) return false;

    RtcDateTime dt(year, month, day, hour, minute, second);
    _rtc.SetDateTime(dt);
    _valid = _rtc.IsDateTimeValid();
    return _valid;
  }

  String formatDateTime(const RtcDateTime& dt) {
    char buf[24];
    snprintf(
      buf, sizeof(buf),
      "%04u-%02u-%02u %02u:%02u:%02u",
      dt.Year(), dt.Month(), dt.Day(),
      dt.Hour(), dt.Minute(), dt.Second()
    );
    return String(buf);
  }

  String nowString() {
    return formatDateTime(now());
  }

  String timeOnlyString() {
    RtcDateTime dt = now();
    char buf[12];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", dt.Hour(), dt.Minute(), dt.Second());
    return String(buf);
  }

private:
  ThreeWire _wire;
  RtcDS1302<ThreeWire> _rtc;
  bool _valid = false;
};
