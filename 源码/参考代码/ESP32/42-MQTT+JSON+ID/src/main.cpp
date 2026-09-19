#include <Arduino.h>
#include "my_config.h"
#include "mqtt.h"
#include "led1.h"
#include "key1.h"

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

    // 连接MQTT
    mqtt_connect();
}

// 根据需要控制各个外设
void handle_device_controls(int id, String device, String key, String value)
{
    // 如果不是发送给自己的消息，则忽略
    if (id != ID)
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
}
