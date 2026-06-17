#include "Config.h"
#include "PwmController.h"
#include "Ina226Monitor.h"
#include "Max31856Sensor.h"
#include "TempPidController.h"
#include "Ds1302Clock.h"
#include "SdCsvLogger.h"
#include "OledStatus.h"

#include <Wire.h>
#include <SPI.h>
#include <math.h>

// =========================
// 全局对象
// =========================
PwmController pwm;
Ina226Monitor ina226;
TempPidController pid;
SdCsvLogger logger;
Ds1302Clock rtc(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN);
OledStatus oled;

Max31856Sensor tc1(
  MAX1_CS_PIN,
  MAX1_MOSI_PIN,
  MAX1_MISO_PIN,
  MAX1_SCK_PIN,
  TC1_TYPE
);

Max31856Sensor tc2(
  MAX2_CS_PIN,
  MAX2_MOSI_PIN,
  MAX2_MISO_PIN,
  MAX2_SCK_PIN,
  TC2_TYPE
);

// =========================
// 控制状态
// =========================
bool autoMode = true;
bool logEnabled = true;

float filtAmbientC = NAN;
float filtMaterialC = NAN;

float lastAmbientC = NAN;
float lastMaterialC = NAN;
float lastSetpointC = NAN;
float lastErrorC = NAN;
float lastPidOutPct = 0.0f;

String cmdLine;
// =========================
// SD 日志均值统计
// =========================
struct LogAverageSnapshot {
  uint32_t sampleCount = 0;

  Ina226Monitor::Data power;

  float tAmb = NAN;
  float tSample = NAN;
  float setpoint = NAN;
  float error = NAN;
  float pidOutPct = NAN;

  int pwmDuty = 0;
  float pwmPct = 0.0f;

  uint8_t fault1 = 0;
  uint8_t fault2 = 0;
};

class LogAverager {
public:
  void reset() {
    sampleCount = 0;

    powerCount = 0;
    busVSum = 0.0;
    shuntmVSum = 0.0;
    currentASum = 0.0;
    powerWSum = 0.0;

    ambCount = 0;
    sampleTempCount = 0;
    setpointCount = 0;
    errorCount = 0;
    pidOutCount = 0;

    ambSum = 0.0;
    sampleTempSum = 0.0;
    setpointSum = 0.0;
    errorSum = 0.0;
    pidOutSum = 0.0;

    pwmDutySum = 0.0;
    pwmPctSum = 0.0;

    fault1Or = 0;
    fault2Or = 0;
  }

  void add(
    const Ina226Monitor::Data& i,
    float tAmb,
    float tSample,
    float setpoint,
    float error,
    float pidOutPct,
    int pwmDuty,
    float pwmPct,
    uint8_t fault1,
    uint8_t fault2
  ) {
    sampleCount++;

    pwmDutySum += pwmDuty;
    pwmPctSum += pwmPct;

    if (i.ok) {
      powerCount++;
      busVSum += i.busV;
      shuntmVSum += i.shuntmV;
      currentASum += i.currentA;
      powerWSum += i.powerW;
    }

    if (!isnan(tAmb)) {
      ambCount++;
      ambSum += tAmb;
    }

    if (!isnan(tSample)) {
      sampleTempCount++;
      sampleTempSum += tSample;
    }

    if (!isnan(setpoint)) {
      setpointCount++;
      setpointSum += setpoint;
    }

    if (!isnan(error)) {
      errorCount++;
      errorSum += error;
    }

    if (!isnan(pidOutPct)) {
      pidOutCount++;
      pidOutSum += pidOutPct;
    }

    fault1Or |= fault1;
    fault2Or |= fault2;
  }

  bool snapshot(LogAverageSnapshot& out) const {
    if (sampleCount == 0) {
      return false;
    }

    out.sampleCount = sampleCount;

    out.power.ok = powerCount > 0;

    if (powerCount > 0) {
      out.power.busV = busVSum / powerCount;
      out.power.shuntmV = shuntmVSum / powerCount;
      out.power.currentA = currentASum / powerCount;
      out.power.powerW = powerWSum / powerCount;
    } else {
      out.power.busV = 0.0f;
      out.power.shuntmV = 0.0f;
      out.power.currentA = 0.0f;
      out.power.powerW = 0.0f;
    }

    out.tAmb = ambCount > 0 ? ambSum / ambCount : NAN;
    out.tSample = sampleTempCount > 0 ? sampleTempSum / sampleTempCount : NAN;
    out.setpoint = setpointCount > 0 ? setpointSum / setpointCount : NAN;
    out.error = errorCount > 0 ? errorSum / errorCount : NAN;
    out.pidOutPct = pidOutCount > 0 ? pidOutSum / pidOutCount : NAN;

    out.pwmDuty = (int)(pwmDutySum / sampleCount + 0.5);
    out.pwmPct = pwmPctSum / sampleCount;

    out.fault1 = fault1Or;
    out.fault2 = fault2Or;

    return true;
  }

private:
  uint32_t sampleCount = 0;

  uint32_t powerCount = 0;
  double busVSum = 0.0;
  double shuntmVSum = 0.0;
  double currentASum = 0.0;
  double powerWSum = 0.0;

  uint32_t ambCount = 0;
  uint32_t sampleTempCount = 0;
  uint32_t setpointCount = 0;
  uint32_t errorCount = 0;
  uint32_t pidOutCount = 0;

  double ambSum = 0.0;
  double sampleTempSum = 0.0;
  double setpointSum = 0.0;
  double errorSum = 0.0;
  double pidOutSum = 0.0;

  double pwmDutySum = 0.0;
  double pwmPctSum = 0.0;

  uint8_t fault1Or = 0;
  uint8_t fault2Or = 0;
};

LogAverager logAvg;
// =========================
// 工具函数
// =========================
float lowpass(float prev, float x, float alpha) {
  if (isnan(prev)) return x;
  return prev + alpha * (x - prev);
}

bool validTempSample(const Max31856Sensor::Data& d) {
  if (!d.ok) return false;
  if (d.fault != 0) return false;
  if (isnan(d.tcTempC)) return false;
  return true;
}

String fmtFloat(float v, int digits = 3) {
  if (isnan(v)) return "nan";
  return String(v, digits);
}

String buildCsvLine(
  const String& dtStr,
  uint32_t timeMs,
  bool modeAuto,
  int pwmDuty,
  float pwmPct,
  const Ina226Monitor::Data& i,
  float tAmb,
  float tMat,
  float setpoint,
  float error,
  float pidOut,
  uint8_t f1,
  uint8_t f2
) {
  String s;
  s.reserve(220);

  s += dtStr; s += ",";
  s += String(timeMs); s += ",";
  s += (modeAuto ? "AUTO" : "MANUAL"); s += ",";
  s += String(pwmDuty); s += ",";
  s += fmtFloat(pwmPct, 2); s += ",";
  s += (i.ok ? fmtFloat(i.busV, 3) : "nan"); s += ",";
  s += (i.ok ? fmtFloat(i.currentA, 4) : "nan"); s += ",";
  s += (i.ok ? fmtFloat(i.powerW, 4) : "nan"); s += ",";
  s += fmtFloat(tAmb, 3); s += ",";
  s += fmtFloat(tMat, 3); s += ",";
  s += fmtFloat(setpoint, 3); s += ",";
  s += fmtFloat(error, 3); s += ",";
  s += fmtFloat(pidOut, 2); s += ",";
  s += String(f1); s += ",";
  s += String(f2);

  return s;
}

bool parseSetTime(
  const String& line,
  int& year,
  int& month,
  int& day,
  int& hour,
  int& minute,
  int& second
) {
  if (!line.startsWith("settime ")) return false;

  String body = line.substring(8);
  body.trim();

  if (body.length() != 19) return false;
  if (body.charAt(4) != '-' || body.charAt(7) != '-' || body.charAt(10) != ' ' ||
      body.charAt(13) != ':' || body.charAt(16) != ':') {
    return false;
  }

  year   = body.substring(0, 4).toInt();
  month  = body.substring(5, 7).toInt();
  day    = body.substring(8, 10).toInt();
  hour   = body.substring(11, 13).toInt();
  minute = body.substring(14, 16).toInt();
  second = body.substring(17, 19).toInt();

  return true;
}

// =========================
// 串口命令
// =========================
void printHelp() {
  Serial.println("========================================");
  Serial.println("手动控制:");
  Serial.println("  pwm 0~255");
  Serial.print("  pct 0~");
  Serial.println(PWM_PERCENT_LIMIT, 0);
  Serial.println("  on");
  Serial.println("  off");
  Serial.println("");

  Serial.println("PID控制:");
  Serial.println("  auto on");
  Serial.println("  auto off");
  Serial.println("  kp <value>");
  Serial.println("  ki <value>");
  Serial.println("  kd <value>");
  Serial.println("  deadband <value>");
  Serial.println("  reseti");
  Serial.println("");

  Serial.println("RTC / 日志:");
  Serial.println("  time");
  Serial.println("  settime YYYY-MM-DD HH:MM:SS");
  Serial.println("  log on");
  Serial.println("  log off");
  Serial.println("  status");
  Serial.println("  help");
  Serial.println("========================================");
}

void printStatus() {
  Serial.println("--------------- STATUS ---------------");

  Serial.print("mode      = ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");

  Serial.print("rtc now   = ");
  Serial.println(rtc.nowString());

  Serial.print("sd ready  = ");
  Serial.println(logger.ready() ? "YES" : "NO");

  Serial.print("oled      = ");
  Serial.println(oled.ready() ? "YES" : "NO");

  Serial.print("log       = ");
  Serial.println(logEnabled ? "ON" : "OFF");

  Serial.print("Kp        = ");
  Serial.println(pid.kp(), 4);
  Serial.print("Ki        = ");
  Serial.println(pid.ki(), 4);
  Serial.print("Kd        = ");
  Serial.println(pid.kd(), 4);
  Serial.print("deadband  = ");
  Serial.println(pid.deadband(), 4);

  Serial.print("PWM duty  = ");
  Serial.print(pwm.duty());
  Serial.print(" / ");
  Serial.print(pwm.maxDuty());
  Serial.print("  (");
  Serial.print(pwm.percent(), 1);
  Serial.println("%)");

  Serial.print("T_amb     = ");
  Serial.println(lastAmbientC, 3);
  Serial.print("T_sample  = ");
  Serial.println(lastMaterialC, 3);
  Serial.print("setpoint  = ");
  Serial.println(lastSetpointC, 3);
  Serial.print("error     = ");
  Serial.println(lastErrorC, 3);
  Serial.print("pid out   = ");
  Serial.println(lastPidOutPct, 3);
  Serial.println("--------------------------------------");
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  String lower = line;
  lower.toLowerCase();

  if (lower == "help") {
    printHelp();
    return;
  }

  if (lower == "status") {
    printStatus();
    return;
  }

  if (lower == "time") {
    Serial.print("RTC time: ");
    Serial.println(rtc.nowString());
    return;
  }

  int year, month, day, hour, minute, second;
  if (parseSetTime(line, year, month, day, hour, minute, second)) {
    if (rtc.setDateTime(year, month, day, hour, minute, second)) {
      Serial.print("OK: RTC set to ");
      Serial.println(rtc.nowString());
    } else {
      Serial.println("RTC set failed");
    }
    return;
  }

  if (lower == "auto on") {
    autoMode = true;
    pid.reset();
    Serial.println("OK: AUTO ON");
    return;
  }

  if (lower == "auto off") {
    autoMode = false;
    pid.reset();
    Serial.println("OK: AUTO OFF");
    return;
  }

  if (lower == "reseti") {
    pid.reset();
    Serial.println("OK: PID integral reset");
    return;
  }

  if (lower == "log on") {
    logEnabled = true;
    Serial.println("OK: LOG ON");
    return;
  }

  if (lower == "log off") {
    logEnabled = false;
    Serial.println("OK: LOG OFF");
    return;
  }

  if (lower.startsWith("kp ")) {
    float v = lower.substring(3).toFloat();
    pid.setTunings(v, pid.ki(), pid.kd());
    Serial.print("OK: Kp=");
    Serial.println(pid.kp(), 4);
    return;
  }

  if (lower.startsWith("ki ")) {
    float v = lower.substring(3).toFloat();
    pid.setTunings(pid.kp(), v, pid.kd());
    Serial.print("OK: Ki=");
    Serial.println(pid.ki(), 4);
    return;
  }

  if (lower.startsWith("kd ")) {
    float v = lower.substring(3).toFloat();
    pid.setTunings(pid.kp(), pid.ki(), v);
    Serial.print("OK: Kd=");
    Serial.println(pid.kd(), 4);
    return;
  }

  if (lower.startsWith("deadband ")) {
    float v = lower.substring(9).toFloat();
    if (v < 0) v = 0;
    pid.setDeadband(v);
    Serial.print("OK: deadband=");
    Serial.println(pid.deadband(), 4);
    return;
  }

  if (lower == "on") {
    autoMode = false;
    pwm.setPercent(PWM_PERCENT_LIMIT);
    Serial.print("OK: MANUAL PWM=");
    Serial.print(pwm.duty());
    Serial.print(" (");
    Serial.print(pwm.percent(), 1);
    Serial.println("%)");
    return;
  }

  if (lower == "off") {
    autoMode = false;
    pwm.setDuty(0);
    Serial.println("OK: MANUAL PWM=0");
    return;
  }

  if (lower.startsWith("pwm ")) {
    int val = lower.substring(4).toInt();
    autoMode = false;
    pwm.setDuty(val);
    Serial.print("OK: MANUAL PWM=");
    Serial.println(pwm.duty());
    return;
  }

  if (lower.startsWith("pct ")) {
    float val = lower.substring(4).toFloat();
    autoMode = false;
    pwm.setPercent(val);
    Serial.print("OK: MANUAL PWM=");
    Serial.print(pwm.duty());
    Serial.print(" (");
    Serial.print(pwm.percent(), 1);
    Serial.println("%)");
    return;
  }

  Serial.println("未知命令，输入 help 查看");
}

void updateSerialCommand() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      cmdLine.trim();
      if (cmdLine.length() > 0) {
        handleCommand(cmdLine);
        cmdLine = "";
      }
    } else {
      cmdLine += c;
    }
  }
}

// =========================
// setup
// =========================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("System start");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C ok");

  if (oled.begin()) {
    Serial.println("OLED ok");
  } else {
    Serial.println("OLED init failed");
  }

  pinMode(MAX1_CS_PIN, OUTPUT);
  pinMode(MAX2_CS_PIN, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(MAX1_CS_PIN, HIGH);
  digitalWrite(MAX2_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);

  if (!pwm.begin(PWM_PIN, PWM_FREQ, PWM_RESOLUTION)) {
    Serial.println("PWM init failed");
    while (1) delay(1000);
  }
  Serial.println("PWM ok");

  if (!ina226.begin(Wire, INA226_ADDR, SHUNT_RESISTOR_OHM)) {
    Serial.println("INA226 init failed");
    while (1) delay(1000);
  }
  Serial.println("INA226 ok");

  if (!tc1.begin()) {
    Serial.println("MAX31856 #1 init failed");
    while (1) delay(1000);
  }
  Serial.println("MAX31856 #1 ok");

  if (!tc2.begin()) {
    Serial.println("MAX31856 #2 init failed");
    while (1) delay(1000);
  }
  Serial.println("MAX31856 #2 ok");

  if (!rtc.begin()) {
    Serial.println("RTC init failed");
  } else {
    Serial.print("RTC ok: ");
    Serial.println(rtc.nowString());
  }

  if (!logger.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN, "/log.csv")) {
    Serial.println("SD init failed");
  } else {
    Serial.println("SD ok -> /log.csv");
  }

  pid.setOutputLimits(0.0f, PWM_PERCENT_LIMIT);
  pid.setTunings(20.0f, 0.25f, 0.0f);
  pid.setDeadband(0.10f);
  pid.reset();

  pwm.setDuty(0);
  printHelp();
}

// =========================
// loop
// =========================
void loop() {
  updateSerialCommand();

  static uint32_t lastPrint = 0;
static uint32_t lastControl = 0;
static uint32_t lastLog = 0;
static uint32_t lastLogSample = 0;


  auto i = ina226.read();
  auto a = tc1.read();
  auto b = tc2.read();

  Max31856Sensor::Data ambData = TC1_IS_AMBIENT ? a : b;
  Max31856Sensor::Data matData = TC1_IS_AMBIENT ? b : a;

  bool ambOk = validTempSample(ambData);
  bool matOk = validTempSample(matData);

  if (ambOk) filtAmbientC = lowpass(filtAmbientC, ambData.tcTempC, TEMP_FILTER_ALPHA);
  if (matOk) filtMaterialC = lowpass(filtMaterialC, matData.tcTempC, TEMP_FILTER_ALPHA);

  lastAmbientC = filtAmbientC;
  lastMaterialC = filtMaterialC;

  // PID 控制
  if (millis() - lastControl >= CONTROL_INTERVAL_MS) {
    float dtSec = (millis() - lastControl) / 1000.0f;
    lastControl = millis();

    if (autoMode) {
      if (ambOk && matOk && !isnan(filtAmbientC) && !isnan(filtMaterialC)) {
        float setpoint = filtAmbientC;
        float input = filtMaterialC;
        float outPct = pid.compute(setpoint, input, dtSec);

        pwm.setPercent(outPct);

        lastSetpointC = setpoint;
        lastErrorC = setpoint - input;
        lastPidOutPct = outPct;
      } else {
        pwm.setDuty(0);
        pid.reset();
        lastPidOutPct = 0.0f;
      }
    } else {
      lastSetpointC = filtAmbientC;
      if (!isnan(filtAmbientC) && !isnan(filtMaterialC)) {
        lastErrorC = filtAmbientC - filtMaterialC;
      } else {
        lastErrorC = NAN;
      }
      lastPidOutPct = pwm.percent();
    }
  }
// 日志均值采样：每 LOG_SAMPLE_INTERVAL_MS 采一次当前状态
uint32_t nowMs = millis();

if (lastLogSample == 0) {
  lastLogSample = nowMs;
}

if (nowMs - lastLogSample >= LOG_SAMPLE_INTERVAL_MS) {
  lastLogSample = nowMs;

  logAvg.add(
    i,
    lastAmbientC,
    lastMaterialC,
    lastSetpointC,
    lastErrorC,
    lastPidOutPct,
    pwm.duty(),
    pwm.percent(),
    a.fault,
    b.fault
  );
}
  // SD 记录：每 LOG_INTERVAL_MS 写一次“窗口平均值”
if (lastLog == 0) {
  lastLog = nowMs;
}

if (nowMs - lastLog >= LOG_INTERVAL_MS) {
  // 尽量保持固定节奏，减少时间漂移
  lastLog += LOG_INTERVAL_MS;

  // 如果主循环被严重阻塞，避免连续补写多条
  if (nowMs - lastLog > LOG_INTERVAL_MS) {
    lastLog = nowMs;
  }

  LogAverageSnapshot avg;

  if (logAvg.snapshot(avg)) {
    if (logEnabled && logger.ready()) {
      String dtStr = rtc.nowString();

      String line = buildCsvLine(
        dtStr,
        millis(),
        autoMode,
        avg.pwmDuty,
        avg.pwmPct,
        avg.power,
        avg.tAmb,
        avg.tSample,
        avg.setpoint,
        avg.error,
        avg.pidOutPct,
        avg.fault1,
        avg.fault2
      );

      if (!logger.appendLine(line)) {
        Serial.println("SD write failed");
      }
    }

    // 无论当前是否写入 SD，都重置统计窗口；
    // 这样 log off 期间不会把很久以前的数据混进下一条。
    logAvg.reset();
  }
}


  // OLED 显示
float powerDensityWm2 = NAN;

if (i.ok && HEATER_AREA_M2 > 0.0f) {
  powerDensityWm2 = i.powerW / HEATER_AREA_M2;
}

String oledTime = rtc.nowString();

oled.update(
  lastAmbientC,
  lastMaterialC,
  i.powerW,
  powerDensityWm2,
  i.ok,
  logEnabled && logger.ready(),
  pwm.percent(),
  autoMode,
  oledTime.c_str()
);


  // 串口打印
  if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = millis();

    Serial.print("TIME=");
    Serial.print(rtc.nowString());

    Serial.print(" | MODE=");
    Serial.print(autoMode ? "AUTO" : "MANUAL");

    Serial.print(" | PWM=");
    Serial.print(pwm.duty());
    Serial.print(" (");
    Serial.print(pwm.percent(), 1);
    Serial.print("%)");

    if (i.ok) {
      Serial.print(" | BusV=");
      Serial.print(i.busV, 3);
      Serial.print("V");

      Serial.print(" | I=");
      Serial.print(i.currentA, 3);
      Serial.print("A");

      Serial.print(" | P=");
      Serial.print(i.powerW, 3);
      Serial.print("W");
    } else {
      Serial.print(" | INA226 fail");
    }

    Serial.print(" | T_amb=");
    if (isnan(lastAmbientC)) Serial.print("NaN");
    else Serial.print(lastAmbientC, 3);
    Serial.print("C");

    Serial.print(" | T_sample=");
    if (isnan(lastMaterialC)) Serial.print("NaN");
    else Serial.print(lastMaterialC, 3);
    Serial.print("C");

    Serial.print(" | SP=");
    if (isnan(lastSetpointC)) Serial.print("NaN");
    else Serial.print(lastSetpointC, 3);
    Serial.print("C");

    Serial.print(" | ERR=");
    if (isnan(lastErrorC)) Serial.print("NaN");
    else Serial.print(lastErrorC, 3);
    Serial.print("C");

    Serial.print(" | PID=");
    Serial.print(lastPidOutPct, 2);
    Serial.print("%");

    Serial.print(" | LOG=");
    Serial.print((logEnabled && logger.ready()) ? "ON" : "OFF");

    Serial.print(" | F1=");
    tc1.printFault(Serial, a.fault);

    Serial.print(" | F2=");
    tc2.printFault(Serial, b.fault);

    Serial.println();
  }
}