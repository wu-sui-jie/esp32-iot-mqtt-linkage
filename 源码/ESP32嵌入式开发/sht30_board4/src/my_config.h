#pragma once

// ============================================================
//  4 号板：温湿度传感器 SHT30
//
//  板号严格按协议表 7：4 号板 = sht30
//  本板是纯传感器板，只有周期上报，没有可控设备。
// ============================================================

// ================ 固定的配置（所有设备都相同） ================
// TODO 整合时统一改为协议附录 A 的 IOT-SZCS01 / iot-szcs01
//
// 【注意】这里叫 WIFI_SSID / WIFI_PASSWORD，不叫协议附录 A 里写的
// SSID / PASSWORD。原因是宏名会全局替换：ESP32 的 WiFi.h 里有
// 一个方法就叫 SSID()，一旦 #define SSID "..."，那行声明会被替换成
//     String "Kris"() const;
// 直接编译报错 "expected unqualified-id before string constant"。
// 课堂例程没踩到，是因为它们把 #include <WiFi.h> 写在了
// #include "my_config.h" 前面；而本工程的 mqtt_proto.h 是先包含
// my_config.h 的，所以必须改名，不能靠包含顺序。Mate 50 pro ktxyj4869
#define WIFI_SSID "wusuijie"             // WiFi账号
#define WIFI_PASSWORD "11111111"    // WiFi密码
#define mqttServer "yunyismart.tech" // MQTT服务器地址
#define mqttPort 1883                // MQTT服务器端口号

// ================ 本组编号与主题定义 ================
// 前缀 = 课程空间前缀 202609_PP2 + 组号 1
#define GROUP_ID 1
#define TOPIC_CMD "202609_PP2/1/cmd"       // 下行：各平台 → 各板（本板只订阅这个）
#define TOPIC_REPORT "202609_PP2/1/report" // 上行：各板 → 各平台
#define TOPIC_ONLINE "202609_PP2/1/online" // 上行：上线下线，Retain

// ================ 本设备板号（1~8，按协议表 7） ================
#define ID 4

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_SHT30 1

// ================ 设备标识（协议表 7） ================
#define DEV_SHT30 "sht30"

// ================ SHT30 硬件参数 ================
// 已对照厂商例程《10. 温湿度传感器实验》确认
#define SHT30_ADDR 0x44 // I2C 从机地址
#define SHT30_SDA 21    // I2C 数据线
#define SHT30_SCL 22    // I2C 时钟线

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。详见 readme「注意事项 A3」。
#define MQTT_BUF_SIZE 1024
