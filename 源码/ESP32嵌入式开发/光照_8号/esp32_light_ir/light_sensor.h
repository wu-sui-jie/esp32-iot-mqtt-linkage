#pragma once

#include <Arduino.h>
#include "my_config.h"

#if HAS_LIGHT_SENSOR

// ============================================================
//  8 号板：光照度传感器模块
//
//  协议表 15：dat illuminance（单位 lx，保留 1 位小数，每 2 秒一次）
//
//  支持两种传感器，上电自动识别：
//    BH1750（I2C，直接给出 lx）/ 光敏电阻（ADC，粗略换算）
// ============================================================

// 上电初始化：识别传感器类型、配置引脚或 I2C
void light_sensor_init();

// 主循环里每次都要调用：周期采集与上报
void light_sensor_loop();

// 最近一次有效的光照度读数（lx）。还没读到有效值时为 -1
float light_sensor_lux();

// 是否识别到 BH1750（false 表示在用光敏电阻）
bool light_sensor_is_bh1750();

#endif
