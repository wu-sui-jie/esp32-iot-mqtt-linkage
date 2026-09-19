#pragma once
#include <Arduino.h>
#include "my_config.h"

#if HAS_LED1

// LED1初始化
void led1_init();

// LED1设置
void led1_set(bool open);

// LED1的循环
void led1_loop();

#endif