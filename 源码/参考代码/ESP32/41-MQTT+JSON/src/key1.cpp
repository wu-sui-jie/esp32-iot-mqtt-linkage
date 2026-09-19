#include "key1.h"
#include "mqtt.h"

// 用宏来控制是否编译，从而减小ROM占用
#if HAS_KEY1

// 定义KEY1的引脚
const int PIN_KEY1 = 36;

void key1_init()
{
    pinMode(PIN_KEY1, INPUT);
}

// KEY1是否已经按下
bool key1_pressed()
{
    return digitalRead(PIN_KEY1) ? false : true;
}

// KEY1上次是否已经按下
static bool key1_pressed_last = false;

void key1_loop()
{
    // 如果上次没有按下
    if (!key1_pressed_last)
    {
        // 现在按下了
        if (key1_pressed())
        {
            key1_pressed_last = true;

            mqtt_post("key1", "state", "pressed");
        }
    }
    // 如果上次是按下的
    else
    {
        // 现在松开了
        if (!key1_pressed())
        {
            key1_pressed_last = false;

            mqtt_post("key1", "state", "released");
        }
    }
}

#endif