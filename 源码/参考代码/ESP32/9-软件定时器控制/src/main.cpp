#include <Arduino.h>
#include <Ticker.h>

// 定义LED1的引脚
const int PIN_LED1 = 2;

// 定时器处理函数
void timer_callback()
{
    // LED灯闪烁
    static uint32_t state = 0;
    state ^= 1;
    digitalWrite(PIN_LED1, state);
}

Ticker timer(timer_callback, 500, 0, MILLIS);

void setup()
{
    // 初始化 LED1 
    pinMode(PIN_LED1, OUTPUT);

    // 定时器开启
    timer.start();
}

void loop()
{
    timer.update();
}
