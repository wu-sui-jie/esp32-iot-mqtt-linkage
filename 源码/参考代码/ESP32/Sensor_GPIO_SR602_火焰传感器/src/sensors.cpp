#include "Arduino.h"
#include "sensors.h"

// 红外人体感应传感器初始化和读取实现
void SR602_init() {
  pinMode(SR602_PIN, INPUT);
}

bool readSR602() {
  return digitalRead(SR602_PIN) == HIGH; // 有人时为HIGH
}

