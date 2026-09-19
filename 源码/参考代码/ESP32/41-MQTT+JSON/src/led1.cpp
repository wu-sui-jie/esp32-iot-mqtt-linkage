#include "led1.h"

// 用宏来控制是否编译，从而减小ROM占用
#if HAS_LED1

// 定义LED1的引脚
const int PIN_LED1 = 2;

// LED的控制字
static bool led1_state = false;

// LED1初始化
void led1_init()
{
    pinMode(PIN_LED1, OUTPUT);
}

// LED1设置
void led1_set(bool open)
{
    led1_state = open;
}

// LED1的循环
void led1_loop()
{
    digitalWrite(PIN_LED1, led1_state ? LOW : HIGH);
}

#endif