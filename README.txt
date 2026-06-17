RadiativePidLogger 多文件 Arduino 工程
=====================================

使用方法：
1. 把整个 RadiativePidLogger 文件夹放到 Arduino sketchbook 目录。
2. Arduino IDE 打开 RadiativePidLogger/RadiativePidLogger.ino。
3. 确认这些库已安装：
   - Adafruit MAX31856 library
   - Rtc by Makuna
   - SD / FS / SPI / Wire（ESP32 Arduino 自带）
4. OLED 使用项目里的 OledStatus.cpp + font.h，不使用 Adafruit_SSD1306。
5. 改引脚只改 Config.h。

当前接线参数在 Config.h：
- INA226 I2C: SDA=37, SCL=38
- OLED: SDA=37, SCL=38，使用软件 I2C 写屏，写完后恢复 Wire
- PWM: GPIO36
- MAX1: CS=13, MOSI=15, MISO=16, SCK=17
- MAX2: CS=12, MOSI=15, MISO=16, SCK=17
- SD: CS=14, MOSI=10, MISO=11, SCK=9
- DS1302: CLK=21, DAT=20, RST=19

串口命令：
- settime YYYY-MM-DD HH:MM:SS
- time
- auto on / auto off
- log on / log off
- kp <value>, ki <value>, kd <value>
- deadband <value>
- reseti
- pct <0~60>
- pwm <0~255>
- on / off
- status

CSV 文件：
- /log.csv
- datetime,time_ms,mode,pwm_duty,pwm_pct,bus_v,current_a,power_w,t_amb_c,t_sample_c,setpoint_c,error_c,pid_out_pct,fault1,fault2

注意：
- 如果 OLED 不亮，请先确认之前可运行的 font.h 已在同一工程文件夹下。
- 如果 INA226 读数异常，先暂时把 loop 里的 oled.update 调用注释掉，验证是否是 OLED 软件 I2C 与硬件 I2C 恢复问题。
