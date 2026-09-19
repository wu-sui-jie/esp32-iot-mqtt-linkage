#pragma once

// ============================================================
//  2 号板：继电器（风扇）模块 fan
//
//  板号严格按协议表 7：2 号板 = 继电器模块（风扇）。
//  本板是执行器，收 cmd 执行、回 ack，没有周期数据要上报。
// ============================================================

// ================ 固定的配置（所有设备都相同） ================
// 【注意】这里叫 WIFI_SSID / WIFI_PASSWORD，不叫协议附录 A 里写的
// SSID / PASSWORD。原因是宏名会全局替换：ESP32 的 WiFi.h 里有
// 一个方法就叫 SSID()，一旦 #define SSID "..."，那行声明会被替换成
//     String "wusuijie"() const;
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
#define BOARD_ID 2
#define BOARD_ID_STR "2"

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_FAN 1

// ================ 设备标识（协议表 7） ================
#define DEV_FAN "fan"

// ================ 风扇硬件参数 ================
// 已对照课程例程《3-继电器控制》与实验箱接线确认：继电器信号线接 GPIO14。
// 风扇是感性负载，继电器模块自带续流二极管，ESP32 侧不需要额外保护。
#define FAN_PIN 14       // 继电器信号线
#define FAN_CHANNEL 0    // LEDC 通道
#define FAN_FREQ 5000    // PWM 频率 5 kHz
#define FAN_RESOLUTION 8 // 8 位分辨率，占空比 0~255

// ================ 协议层开关（定义见 mqtt_proto.h） ================
// 本板动作是瞬时的（一个 ledcWrite 就完成），收到命令立即回执。
#define ACK_AFTER_DONE 0    // 无长动作，立即回执
#define SUB_EXTRA_REPORT 0  // 联动开关，阶段 4 打开
#define SUB_EXTRA_ONLINE 0  // 不需要旁听上下线

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
