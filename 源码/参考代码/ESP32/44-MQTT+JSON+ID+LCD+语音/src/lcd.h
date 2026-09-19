#pragma once
#include <Arduino.h>
#include "my_config.h"

#if HAS_LCD

// LCD初始化
void lcd_init();

// LCD在固定位置显示ID编号
void lcd_showID();

// LCD显示字符串
void lcd_showStr(String str);

// LCD的循环
void lcd_loop();

#endif