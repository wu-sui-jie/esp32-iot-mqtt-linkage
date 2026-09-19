#include "Arduino.h"
#include "sensors.h"

// 红外对射传感器初始化和读取实现
void IRBarrier_init() {
  pinMode(IR_BARRIER_PIN, INPUT);
}

bool readIRBarrier() {
  return digitalRead(IR_BARRIER_PIN) == HIGH; // 有遮挡时为HIGH
}

