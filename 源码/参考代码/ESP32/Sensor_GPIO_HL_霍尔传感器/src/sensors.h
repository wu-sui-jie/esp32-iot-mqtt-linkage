#ifndef _CONFIG_H
#define _CONFIG_H

#include "Arduino.h"
#include <ArduinoJson.h>

#define HALL_SENSOR_PIN 14  // 霍尔传感器引脚

// 霍尔传感器相关函数
void HallSensor_init();
bool readHallDigital();

#endif
