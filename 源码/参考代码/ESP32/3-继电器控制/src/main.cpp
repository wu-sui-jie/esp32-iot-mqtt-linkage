#include <Arduino.h>

// 定义继电器1的引脚
const int PIN_RELAY1 = 14;

// 打开 RELAY1
void RELAY1_on()
{
    digitalWrite(PIN_RELAY1, HIGH);
}

// 关闭 RELAY1
void RELAY1_off()
{
    digitalWrite(PIN_RELAY1, LOW);
}

void setup()
{
    // 初始化 RELAY1
    pinMode(PIN_RELAY1, OUTPUT);
    RELAY1_on();
}

void loop()
{
}
