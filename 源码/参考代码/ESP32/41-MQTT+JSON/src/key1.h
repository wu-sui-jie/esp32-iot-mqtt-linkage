#pragma once
#include <Arduino.h>
#include "my_config.h"

#if HAS_KEY1

// KEY1初始化
void key1_init();

// KEY1的循环
void key1_loop();

#endif