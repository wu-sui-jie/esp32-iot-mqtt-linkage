#include "Arduino.h"
#include "sensors.h"

// 霍尔传感器初始化和读取实现
void HallSensor_init() {
  pinMode(HALL_SENSOR_PIN, INPUT);
}

bool readHallDigital() {
  return digitalRead(HALL_SENSOR_PIN) == LOW; // 检测到磁场时为LOW
}

