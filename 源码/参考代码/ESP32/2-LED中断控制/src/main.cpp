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

// 状态机变量
int state = 0;

// 中断函数 (中断回调函数必须位于IRAM中，需要 ICACHE_RAM_ATTR 属性）
ICACHE_RAM_ATTR void stateChange()
{
    // 按键按下时，改变状态机
    switch (state)
    {
    case 0:
        state = 1;
        break;

    case 1:
        state = 2;
        break;

    default:
        state = 0;
        break;
    }
}

void setup()
{
    // 初始化 LED1、KEY1
    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_KEY1, INPUT);

    // 监视KEY1的变化
    attachInterrupt(PIN_KEY1, stateChange, FALLING);
}

void loop()
{
    // 根据状态机做出相应的动作
    switch (state)
    {
    case 0: // 慢速闪烁
        LED1_off();
        delay(1000);
        LED1_on();
        delay(1000);
        break;

    case 1: // 中速闪烁
        LED1_off();
        delay(300);
        LED1_on();
        delay(300);
        break;

    case 2: // 快速闪烁
        LED1_off();
        delay(100);
        LED1_on();
        delay(100);
        break;

    default:
        break;
    }
}