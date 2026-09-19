#pragma once

// ================ 固定的配置（所有设备都相同） ================
#define SSID "IOT-SZCS01"            // WiFi账号
#define PASSWORD "iot-szcs01"        // WiFi密码
#define mqttServer "yunyismart.tech" // MQTT服务器地址
#define mqttPort 1883                // MQTT服务器端口号

// ================ 可变的配置（每个设备都不同） ================

// MQTT消息主题，格式：202609_PP2/{groupID}
#define TOPIC "202609_PP2/15"

// 是否启用外设(LED1, LED2, KEY1, KEY2, FAN, SERVO, 等等)
#define HAS_LED1 1 // LED1
#define HAS_KEY1 1 // 按键1
#define HAS_FAN 1  // 风扇
