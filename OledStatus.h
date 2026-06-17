#pragma once

#include <Arduino.h>

class OledStatus {
public:
  bool begin();

  void update(
    float ambientC,
    float sampleC,
    float powerW,
    float powerDensityWm2,
    bool powerOk,
    bool logOn,
    float pwmPct,
    bool autoMode,
    const char* timeText
  );

  bool ready() const;

private:
  bool _ready = false;
};