#include <Arduino.h>
#include "my_config.h"
#include "mqtt.h"
#include "led1.h"
#include "key1.h"
#include "lcd.h"
#include "tts.h"

void setup()
{
    // 串口初始化，用于调试
    Serial.begin(115200);

#if HAS_LED1
    led1_init();
#endif

#if HAS_KEY1
    key1_init();
#endif

#if HAS_LCD
    lcd_init();
#endif

#if HAS_TTS
    tts_init();
#endif

    // 连接MQTT
    mqtt_connect();
}

// 根据需要控制各个外设
void handle_device_controls(int id, String device, String key, String value)
{
    // ID检查规则
    // 1. 如果是0，则无条件接收，相当于是收到了广播消息
    // 2. 如果等于自身ID，则接收
    // 3. 如果不是0，也不是自身ID，则忽略
    if ((id != 0) && (id != ID))
        return;

#if HAS_LED1
    if (device == "led1")
    {
        if (key == "control")
        {
            if (value == "on")
                led1_set(true);
            else if (value == "off")
                led1_set(false);
        }
    }
#endif

#if HAS_LCD
    if (device == "lcd")
    {
        if (key == "control")
        {
            lcd_showStr(value);
        }
    }
#endif

#if HAS_TTS
    if (device == "tts")
    {
        if (key == "control")
        {
            tts_play(value);
        }
    }
#endif
}

void loop()
{
    if (!mqtt_connected())
    {
        Serial.println("服务器尚未连接...");
        delay(1000);
    }
    mqtt_loop();

    // 处理各个设备的循环

    led1_loop();
    key1_loop();
    lcd_loop();
    tts_loop();
}
