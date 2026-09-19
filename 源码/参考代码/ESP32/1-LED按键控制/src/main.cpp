#include <Arduino.h>

// 定义LED1的引脚
const int PIN_LED1 = 2;
// 定义KEY1的引脚
const int PIN_KEY1 = 36;

// 打开LED1
void LED1_on()
{
    digitalWrite(PIN_LED1, LOW);
}

// 关闭LED1
void LED1_off()
{
    digitalWrite(PIN_LED1, HIGH);
}

// 按键1是否按下？
bool KEY1_Pressed()
{
    return digitalRead(PIN_KEY1) ? false : true;
}

void setup()
{
    // 初始化LED1
    pinMode(PIN_LED1, OUTPUT);
    // 初始化KEY1
    pinMode(PIN_KEY1, INPUT);
}

void loop()
{
    if (KEY1_Pressed())
        LED1_on();
    else
        LED1_off();
}
