#pragma once

#include <Arduino.h>
#include <Adafruit_MAX31856.h>
// =========================
// 加热器面积
// =========================
// 这里填写加热器有效加热面积，单位 cm^2
// 例如 5cm x 5cm = 25.0 cm^2
static const float HEATER_AREA_CM2 = 50.26f;

// 自动换算成 m^2，用于计算 W/m^2
static const float HEATER_AREA_M2 = HEATER_AREA_CM2 * 1.0e-4f;


// =========================
// INA226 / I2C
// =========================
static const uint8_t INA226_ADDR = 0x40;
static const int I2C_SDA_PIN = 37;
static const int I2C_SCL_PIN = 38;
static const float SHUNT_RESISTOR_OHM = 0.1f;

// =========================
// OLED SSD1306
// 独立软件 I2C
// =========================
static const int OLED_SDA_PIN = 39;
static const int OLED_SCL_PIN = 40;
static const int OLED_RST_PIN = -1;

// 0x78 = 0x3C << 1
// 如果 OLED 地址是 0x3D，改成 0x7A
static const uint8_t OLED_I2C_WRITE_ADDR = 0x78;

// 软件 I2C 延时：保持你测试成功的参数
static const uint8_t OLED_I2C_DELAY_US = 30;

// OLED 后台任务设置
static const int OLED_TASK_CORE = 0;
static const int OLED_TASK_PRIORITY = 1;
static const int OLED_TASK_STACK_SIZE = 4096;

// 每刷完一帧后让出 CPU
static const uint32_t OLED_TASK_YIELD_MS = 1;

// =========================
// PWM / MOSFET
// =========================
static const int PWM_PIN = 36;
static const uint32_t PWM_FREQ = 20000;
static const uint8_t PWM_RESOLUTION = 8;
static const float PWM_PERCENT_LIMIT = 60.0f;

// =========================
// MAX31856 #1
// =========================
static const int MAX1_CS_PIN   = 13;
static const int MAX1_MOSI_PIN = 15;
static const int MAX1_MISO_PIN = 16;
static const int MAX1_SCK_PIN  = 17;

// =========================
// MAX31856 #2
// =========================
static const int MAX2_CS_PIN   = 12;
static const int MAX2_MOSI_PIN = 15;
static const int MAX2_MISO_PIN = 16;
static const int MAX2_SCK_PIN  = 17;

static const max31856_thermocoupletype_t TC1_TYPE = MAX31856_TCTYPE_K;
static const max31856_thermocoupletype_t TC2_TYPE = MAX31856_TCTYPE_K;

// true: tc1=环境温度, tc2=样品温度
static const bool TC1_IS_AMBIENT = true;

// =========================
// TF 卡，独立 SPI
// =========================
static const int SD_CS_PIN   = 14;
static const int SD_MOSI_PIN = 10;
static const int SD_MISO_PIN = 11;
static const int SD_SCK_PIN  = 9;

// =========================
// DS1302 RTC
// =========================
static const int RTC_CLK_PIN = 21;
static const int RTC_DAT_PIN = 20;
static const int RTC_RST_PIN = 19;

// =========================
// 周期参数
// =========================
static const uint32_t PRINT_INTERVAL_MS   = 1000;
static const uint32_t CONTROL_INTERVAL_MS = 500;
// SD 记录周期：2 秒写一条
static const uint32_t LOG_INTERVAL_MS = 2000;

// 日志均值采样周期：每 100 ms 累计一次
static const uint32_t LOG_SAMPLE_INTERVAL_MS = 100;

// 温度低通滤波
static const float TEMP_FILTER_ALPHA = 0.25f;