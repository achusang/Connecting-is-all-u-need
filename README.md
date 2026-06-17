# Connecting is all u need

This repository contains the **Arduino** firmware for a low-cost modular embedded platform for outdoor radiative cooling testing. The platform is designed to connect commercial off-the-shelf modules for temperature sensing, heater control, electrical power monitoring, time labeling, local data logging, and on-site status display.

The code was developed for an ESP32-S3-based prototype used in the manuscript:

**A low-cost modular embedded platform for outdoor radiative cooling testing: Connecting is all you need**

## Main functions

The firmware supports:

* Dual thermocouple temperature measurement using two MAX31856 modules
* Ambient and sample temperature assignment through `Config.h`
* INA226-based voltage/current/power monitoring
* PWM heater control through a MOSFET module
* Automatic ambient-temperature tracking using PID control
* Manual PWM control through serial commands
* DS1302 real-time-clock time labeling
* MicroSD CSV data logging
* SSD1306 OLED status display
* Window-averaged logging for temperature, power, PWM, and control variables

## Hardware modules

The prototype uses the following main modules:

* ESP32-S3 development board
* Two MAX31856 thermocouple interface modules
* Two K-type thermocouples
* INA226 voltage/current monitoring module
* N-MOSFET heater driver
* PI film heater
* DS1302 real-time clock module
* MicroSD card adapter
* SSD1306 OLED display
* Dupont jumper wires

The default pin assignments are defined in `Config.h`.

## Default pin configuration

| Function              | Pin/configuration |
| --------------------- | ----------------- |
| INA226 I2C SDA        | GPIO37            |
| INA226 I2C SCL        | GPIO38            |
| OLED software I2C SDA | GPIO39            |
| OLED software I2C SCL | GPIO40            |
| PWM output            | GPIO36            |
| MAX31856 #1 CS        | GPIO13            |
| MAX31856 #2 CS        | GPIO12            |
| MAX31856 SPI MOSI     | GPIO15            |
| MAX31856 SPI MISO     | GPIO16            |
| MAX31856 SPI SCK      | GPIO17            |
| MicroSD CS            | GPIO14            |
| MicroSD MOSI          | GPIO10            |
| MicroSD MISO          | GPIO11            |
| MicroSD SCK           | GPIO9             |
| DS1302 CLK            | GPIO21            |
| DS1302 DAT            | GPIO20            |
| DS1302 RST            | GPIO19            |

## Software requirements

Open the project with Arduino IDE or an ESP32-compatible Arduino build environment.

Required libraries:

* ESP32 Arduino core
* Adafruit MAX31856 library
* Rtc by Makuna
* SD, FS, SPI, and Wire libraries included with the ESP32 Arduino core

The OLED display is driven by the project files `OledStatus.cpp`, `OledStatus.h`, and `OledFont5x7.h`. The firmware does not require the Adafruit SSD1306 library.

## How to use

1. Put all source files in one Arduino sketch folder.
2. Open `RadiativePidLogger.ino` in Arduino IDE.
3. Check and modify the pin assignments and heater area in `Config.h`.
4. Select the appropriate ESP32-S3 board and serial port.
5. Upload the firmware.
6. Open the serial monitor at 115200 baud.
7. Set the RTC time before outdoor testing.
8. Insert a MicroSD card to record `/log.csv`.

## Serial commands

The firmware accepts the following serial commands:

```text
help
status
time
settime YYYY-MM-DD HH:MM:SS

auto on
auto off

kp <value>
ki <value>
kd <value>
deadband <value>
reseti

log on
log off

pwm <0-255>
pct <0-60>
on
off
```

Typical workflow:

```text
settime 2026-05-11 16:00:00
auto on
log on
status
```

Use `auto on` for ambient-temperature tracking. Use `auto off`, `pwm`, `pct`, `on`, and `off` for manual heater control.

## CSV output

The firmware writes data to:

```text
/log.csv
```

The CSV columns are:

```text
datetime,time_ms,mode,pwm_duty,pwm_pct,bus_v,current_a,power_w,t_amb_c,t_sample_c,setpoint_c,error_c,pid_out_pct,fault1,fault2
```

The recorded data include time, control mode, PWM output, electrical measurements, ambient temperature, sample temperature, setpoint, control error, PID output, and MAX31856 fault codes.

## Notes

* The effective heater area is defined in `Config.h` and is used to calculate cooling power density.
* The default PWM frequency is 20 kHz.
* The default PWM limit is 60%.
* The control loop interval is 500 ms.
* CSV logging is performed every 2 s using averaged values.
* The real-time clock should be set before long outdoor tests.
* Pin assignments can be changed in `Config.h` according to the selected ESP32-S3 board.


## Citation

If you use this code or platform, please cite the associated manuscript:

Chen H, Li J, Fan Y, Liu X, Liu J, Wang C. A low-cost modular embedded platform for outdoor radiative cooling testing: Connecting is all you need.

I
