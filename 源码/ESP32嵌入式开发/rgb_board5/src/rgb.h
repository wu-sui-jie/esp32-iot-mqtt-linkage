#pragma once

#include <Arduino.h>
#include "my_config.h"

#if HAS_RGB

// RGB 初始化：上电先全灭
void rgb_init();

// 设置纯色，r / g / b 各 0~255
// 三通道全传 0 即表示熄灭（协议 §5.5：RGB 不单独定义开关命令）
void rgb_apply(uint8_t r, uint8_t g, uint8_t b);

// 当前颜色，用于回执时回发实际状态
uint8_t rgb_get_r();
uint8_t rgb_get_g();
uint8_t rgb_get_b();

#endif
