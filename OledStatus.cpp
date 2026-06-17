#include "OledStatus.h"
#include "Config.h"
#include "OledFont5x7.h"

#include <Arduino.h>
#include <math.h>
#include <pgmspace.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OLED_CMD  0
#define OLED_DATA 1

// =========================
// OLED 数据缓存
// 主程序写缓存，OLED 后台任务读缓存
// =========================
struct OledDataCache {
  float ambientC;
  float sampleC;
  float powerW;
  float powerDensityWm2;
  bool powerOk;
  bool logOn;
  float pwmPct;
  bool autoMode;
  char timeText[24];
};

static OledDataCache oledCache;
static portMUX_TYPE oledMux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t oledTaskHandle = nullptr;
static bool oledTaskRunning = false;

// =========================
// 软件 I2C
// =========================
static void i2cDelay() {
  delayMicroseconds(OLED_I2C_DELAY_US);
}

static void OLED_SCL_LOW() {
  pinMode(OLED_SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(OLED_SCL_PIN, LOW);
}

static void OLED_SCL_HIGH() {
  pinMode(OLED_SCL_PIN, INPUT_PULLUP);
}

static void OLED_SDA_LOW() {
  pinMode(OLED_SDA_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(OLED_SDA_PIN, LOW);
}

static void OLED_SDA_HIGH() {
  pinMode(OLED_SDA_PIN, INPUT_PULLUP);
}

static void I2C_Start() {
  OLED_SDA_HIGH();
  OLED_SCL_HIGH();
  i2cDelay();

  OLED_SDA_LOW();
  i2cDelay();

  OLED_SCL_LOW();
  i2cDelay();
}

static void I2C_Stop() {
  OLED_SCL_LOW();
  OLED_SDA_LOW();
  i2cDelay();

  OLED_SCL_HIGH();
  i2cDelay();

  OLED_SDA_HIGH();
  i2cDelay();
}

static void Send_Byte(uint8_t dat) {
  for (uint8_t i = 0; i < 8; i++) {
    OLED_SCL_LOW();
    i2cDelay();

    if (dat & 0x80) OLED_SDA_HIGH();
    else OLED_SDA_LOW();

    i2cDelay();

    OLED_SCL_HIGH();
    i2cDelay();

    OLED_SCL_LOW();
    i2cDelay();

    dat <<= 1;
  }

  // ACK clock，忽略 ACK
  OLED_SDA_HIGH();
  i2cDelay();

  OLED_SCL_HIGH();
  i2cDelay();

  OLED_SCL_LOW();
  i2cDelay();
}

static void OLED_WR_Byte(uint8_t dat, uint8_t mode) {
  I2C_Start();

  Send_Byte(OLED_I2C_WRITE_ADDR);

  if (mode) Send_Byte(0x40);
  else Send_Byte(0x00);

  Send_Byte(dat);

  I2C_Stop();
}

static void OLED_WriteDataBlock(const uint8_t* data, uint8_t len) {
  I2C_Start();

  Send_Byte(OLED_I2C_WRITE_ADDR);
  Send_Byte(0x40);

  for (uint8_t i = 0; i < len; i++) {
    Send_Byte(data[i]);
  }

  I2C_Stop();
}

// =========================
// OLED 基础控制
// =========================
static void OLED_SetPage(uint8_t page) {
  OLED_WR_Byte(0xB0 + page, OLED_CMD);
}

static void OLED_SetColumn(uint8_t col) {
  OLED_WR_Byte(0x00 + (col & 0x0F), OLED_CMD);
  OLED_WR_Byte(0x10 + ((col >> 4) & 0x0F), OLED_CMD);
}

static void OLED_SetPos(uint8_t col, uint8_t page) {
  OLED_SetPage(page);
  OLED_SetColumn(col);
}

static void OLED_KeepAlive() {
  OLED_WR_Byte(0xAF, OLED_CMD); // display on
  OLED_WR_Byte(0xA4, OLED_CMD); // follow RAM
  OLED_WR_Byte(0xA6, OLED_CMD); // normal display

  OLED_WR_Byte(0x81, OLED_CMD);
  OLED_WR_Byte(0x40, OLED_CMD); // 低亮度
}

static void OLED_ClearScreenOnce() {
  uint8_t blank[128];
  memset(blank, 0x00, sizeof(blank));

  for (uint8_t page = 0; page < 8; page++) {
    OLED_SetPos(0, page);
    OLED_WriteDataBlock(blank, 128);
  }
}

// =========================
// 行缓冲绘制
// =========================
static uint8_t pageBuffer[128];

static void bufferClear() {
  memset(pageBuffer, 0x00, sizeof(pageBuffer));
}

static void bufferDrawChar(uint8_t col, char c) {
  if (c >= 'a' && c <= 'z') {
    c = c - 'a' + 'A';
  }

  if (c < 32 || c > 90) {
    c = ' ';
  }

  if (col >= 123) return;

  uint8_t index = c - 32;

  for (uint8_t i = 0; i < 5; i++) {
    if (col + i < 128) {
      pageBuffer[col + i] = pgm_read_byte(&FONT_5X7[index][i]);
    }
  }

  if (col + 5 < 128) {
    pageBuffer[col + 5] = 0x00;
  }
}

static void bufferDrawString(uint8_t col, const char* str) {
  while (*str && col < 122) {
    bufferDrawChar(col, *str);
    col += 6;
    str++;
  }
}

static void OLED_WriteBufferedPage(uint8_t page) {
  OLED_SetPos(0, page);
  OLED_WriteDataBlock(pageBuffer, 128);
}

static void OLED_RenderPageText(uint8_t page, const char* text) {
  bufferClear();
  bufferDrawString(0, text);
  OLED_WriteBufferedPage(page);
}

// =========================
// 数值格式
// =========================
static void formatFloatValue(
  char* buf,
  size_t size,
  const char* label,
  float value,
  const char* unit,
  uint8_t digits
) {
  if (isnan(value)) {
    snprintf(buf, size, "%s --.-- %s", label, unit);
  } else {
    char fmt[20];
    snprintf(fmt, sizeof(fmt), "%%s %%.%uf %%s", digits);
    snprintf(buf, size, fmt, label, value, unit);
  }
}

// =========================
// OLED 页面刷新
// 继续保持你测试成功的方式：
// 每一行生成完整 pageBuffer，再整行覆盖。
// =========================
static void OLED_RenderValuePage(const OledDataCache& data) {
  char line[32];

  OLED_KeepAlive();

  formatFloatValue(line, sizeof(line), "AMB :", data.ambientC, "C", 2);
  OLED_RenderPageText(0, line);

  formatFloatValue(line, sizeof(line), "SAMP:", data.sampleC, "C", 2);
  OLED_RenderPageText(1, line);

  if (data.powerOk) {
    formatFloatValue(line, sizeof(line), "PWR :", data.powerW, "W", 3);
  } else {
    snprintf(line, sizeof(line), "PWR : --.-- W");
  }
  OLED_RenderPageText(2, line);

  if (data.powerOk && !isnan(data.powerDensityWm2)) {
    formatFloatValue(line, sizeof(line), "PD  :", data.powerDensityWm2, "W/M2", 1);
  } else {
    snprintf(line, sizeof(line), "PD  : --.-- W/M2");
  }
  OLED_RenderPageText(3, line);

  formatFloatValue(line, sizeof(line), "PWM :", data.pwmPct, "%", 1);
  OLED_RenderPageText(4, line);

  if (strlen(data.timeText) > 0) {
    OLED_RenderPageText(5, data.timeText);
  } else {
    OLED_RenderPageText(5, "0000-00-00 00:00:00");
  }

  snprintf(line, sizeof(line), "LOG : %s", data.logOn ? "ON" : "OFF");
  OLED_RenderPageText(6, line);

  snprintf(line, sizeof(line), "MODE: %s", data.autoMode ? "AUTO" : "MAN");
  OLED_RenderPageText(7, line);
}


// =========================
// OLED 后台刷新任务
// =========================
static void oledRefreshTask(void* parameter) {
  while (true) {
    OledDataCache local;

    portENTER_CRITICAL(&oledMux);
    local = oledCache;
    portEXIT_CRITICAL(&oledMux);

    OLED_RenderValuePage(local);

    // 稍微让出 CPU
    vTaskDelay(pdMS_TO_TICKS(OLED_TASK_YIELD_MS));
  }
}

// =========================
// OLED 初始化
// 这组命令保持你测试代码的方向：0xC8 + 0xA1
// =========================
static void OLED_InitRaw() {
  pinMode(OLED_SCL_PIN, INPUT_PULLUP);
  pinMode(OLED_SDA_PIN, INPUT_PULLUP);

  OLED_SCL_HIGH();
  OLED_SDA_HIGH();

  delay(300);

  if (OLED_RST_PIN >= 0) {
    pinMode(OLED_RST_PIN, OUTPUT);
    digitalWrite(OLED_RST_PIN, HIGH);
    delay(100);
    digitalWrite(OLED_RST_PIN, LOW);
    delay(100);
    digitalWrite(OLED_RST_PIN, HIGH);
    delay(100);
  }

  OLED_WR_Byte(0xAE, OLED_CMD); // display off

  OLED_WR_Byte(0x20, OLED_CMD);
  OLED_WR_Byte(0x02, OLED_CMD); // page addressing mode

  OLED_WR_Byte(0xB0, OLED_CMD);

  OLED_WR_Byte(0xC8, OLED_CMD);
  OLED_WR_Byte(0x00, OLED_CMD);
  OLED_WR_Byte(0x10, OLED_CMD);
  OLED_WR_Byte(0x40, OLED_CMD);

  OLED_WR_Byte(0x81, OLED_CMD);
  OLED_WR_Byte(0x40, OLED_CMD); // 低亮度，避免电流太大

  OLED_WR_Byte(0xA1, OLED_CMD);
  OLED_WR_Byte(0xA6, OLED_CMD);

  OLED_WR_Byte(0xA8, OLED_CMD);
  OLED_WR_Byte(0x3F, OLED_CMD);

  OLED_WR_Byte(0xA4, OLED_CMD);

  OLED_WR_Byte(0xD3, OLED_CMD);
  OLED_WR_Byte(0x00, OLED_CMD);

  OLED_WR_Byte(0xD5, OLED_CMD);
  OLED_WR_Byte(0x80, OLED_CMD);

  OLED_WR_Byte(0xD9, OLED_CMD);
  OLED_WR_Byte(0xF1, OLED_CMD);

  OLED_WR_Byte(0xDA, OLED_CMD);
  OLED_WR_Byte(0x12, OLED_CMD);

  OLED_WR_Byte(0xDB, OLED_CMD);
  OLED_WR_Byte(0x40, OLED_CMD);

  OLED_WR_Byte(0x8D, OLED_CMD);
  OLED_WR_Byte(0x14, OLED_CMD);

  OLED_WR_Byte(0xAF, OLED_CMD); // display on

  delay(200);
}

// =========================
// OledStatus 类实现
// =========================
bool OledStatus::begin() {
  OLED_InitRaw();
  OLED_ClearScreenOnce();

  portENTER_CRITICAL(&oledMux);
  oledCache.ambientC = NAN;
  oledCache.sampleC = NAN;
  oledCache.powerW = NAN;
  oledCache.powerDensityWm2 = NAN;
  oledCache.powerOk = false;
  oledCache.logOn = false;
  oledCache.pwmPct = NAN;
  oledCache.autoMode = true;
  strncpy(oledCache.timeText, "0000-00-00 00:00:00", sizeof(oledCache.timeText) - 1);
  oledCache.timeText[sizeof(oledCache.timeText) - 1] = '\0';
  portEXIT_CRITICAL(&oledMux);

  _ready = true;

  // 先刷一帧
  OledDataCache local;
  portENTER_CRITICAL(&oledMux);
  local = oledCache;
  portEXIT_CRITICAL(&oledMux);
  OLED_RenderValuePage(local);

  if (!oledTaskRunning) {
    BaseType_t ok = xTaskCreatePinnedToCore(
      oledRefreshTask,
      "OLEDRefresh",
      OLED_TASK_STACK_SIZE,
      nullptr,
      OLED_TASK_PRIORITY,
      &oledTaskHandle,
      OLED_TASK_CORE
    );

    if (ok == pdPASS) {
      oledTaskRunning = true;
    } else {
      _ready = false;
      return false;
    }
  }

  return true;
}

void OledStatus::update(
  float ambientC,
  float sampleC,
  float powerW,
  float powerDensityWm2,
  bool powerOk,
  bool logOn,
  float pwmPct,
  bool autoMode,
  const char* timeText
) {
  if (!_ready) return;

  portENTER_CRITICAL(&oledMux);
  oledCache.ambientC = ambientC;
  oledCache.sampleC = sampleC;
  oledCache.powerW = powerW;
  oledCache.powerDensityWm2 = powerDensityWm2;
  oledCache.powerOk = powerOk;
  oledCache.logOn = logOn;
  oledCache.pwmPct = pwmPct;
  oledCache.autoMode = autoMode;

  if (timeText != nullptr && strlen(timeText) > 0) {
    strncpy(oledCache.timeText, timeText, sizeof(oledCache.timeText) - 1);
    oledCache.timeText[sizeof(oledCache.timeText) - 1] = '\0';
  } else {
    strncpy(oledCache.timeText, "0000-00-00 00:00:00", sizeof(oledCache.timeText) - 1);
    oledCache.timeText[sizeof(oledCache.timeText) - 1] = '\0';
  }
  portEXIT_CRITICAL(&oledMux);
}

bool OledStatus::ready() const {
  return _ready;
}