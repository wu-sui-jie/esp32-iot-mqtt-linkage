#pragma once

// ============================================================
//  5 号板：RGB 灯 WS2812B
//
//  板号严格按协议表 7：5 号板 = adc / rgb / buzzer
//  本次只实现 rgb，adc 与 buzzer【不实现】，使能宏保留并设为 0。
//  详见 readme.txt「给合代码的人：必读注意事项」C 节。
// ============================================================

// ================ 固定的配置（所有设备都相同） ================
// 【注意】这里叫 WIFI_SSID / WIFI_PASSWORD，不叫协议附录 A 里写的
// SSID / PASSWORD。原因是宏名会全局替换：ESP32 的 WiFi.h 里有
// 一个方法就叫 SSID()，一旦 #define SSID "..."，那行声明会被替换成
//     String "IOT-SZCS01"() const;
// 直接编译报错 "expected unqualified-id before string constant"。
// 课堂例程没踩到，是因为它们把 #include <WiFi.h> 写在了
// #include "my_config.h" 前面；而本工程的 mqtt_proto.h 是先包含
// my_config.h 的，所以必须改名，不能靠包含顺序。
#define WIFI_SSID     "wusuijie"        // WiFi账号
#define WIFI_PASSWORD "11111111"        // WiFi密码
#define MQTT_SERVER   "yunyismart.tech" // MQTT服务器地址
#define MQTT_PORT     1883              // MQTT服务器端口号

// ================ 本组编号与主题定义 ================
// 前缀 = 课程空间前缀 202609_PP2 + 组号 1
#define GROUP_ID 1
#define TOPIC_CMD "202609_PP2/1/cmd"       // 下行：各平台 → 各板（本板只订阅这个）
#define TOPIC_REPORT "202609_PP2/1/report" // 上行：各板 → 各平台
#define TOPIC_ONLINE "202609_PP2/1/online" // 上行：上线下线，Retain

// ================ 本设备板号（1~8，按协议表 7） ================
#define BOARD_ID 5
#define BOARD_ID_STR "5"

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_RGB 1    // RGB 彩灯，本次实现
#define HAS_ADC 0    // 电位器，本次不实现
#define HAS_BUZZER 0 // 蜂鸣器，本次不实现

// ================ 设备标识（协议表 7） ================
#define DEV_RGB "rgb"

// ================ RGB 彩灯的硬件参数 ================
// 已对照原理图《SCH_RGB蜂鸣器电位器_2025-09-23》确认：
//   XL-5050RGBC-WS2812B × 4 (LED1~LED4)，数据线接 IO27。
#if HAS_RGB
#define RGB_PIN 27
#define RGB_COUNT 4
#endif

// ================ 协议层开关（定义见 mqtt_proto.h） ================
// 本板是执行器，RGB 设色是瞬时的，收到命令立即回执；
// 旁听 report / online 是为后续的「光照照明、火焰声光报警」联动预留，
// 本次先置 0，等联动实现时再打开。
#define ACK_AFTER_DONE 0    // 无长动作，立即回执
#define SUB_EXTRA_REPORT 0  // 联动开关，阶段 4 打开
#define SUB_EXTRA_ONLINE 0  // 不需要

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。详见 readme「注意事项 A3」。
#define MQTT_BUF_SIZE 1024
