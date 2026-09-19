#pragma once

#include <Arduino.h>
#include "my_config.h"

#if HAS_SHT30

// SHT30 初始化（I2C 总线）
void sht30_init();

// SHT30 的循环：非阻塞的采集状态机，每 5 秒上报一次温湿度
void sht30_loop();

#endif
