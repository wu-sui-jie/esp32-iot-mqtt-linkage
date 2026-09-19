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

// ================ 温湿度联动的参数 ================
// 4 号板报来的温度或湿度超过开阈值就自动开风扇；回到关阈值以下
// 并持续 LINK_FAN_HOLD_MS 之后才关。
//
// 【开/关阈值必须分开（迟滞）】只用一个阈值的话，读数在阈值附近
// 来回抖动时继电器会"啪嗒啪嗒"反复吸合，既吵又伤触点。
#define LINK_TEMP_ON 30.0f  // 温度达到多少就开风扇（协议范围 -40.0~125.0）
#define LINK_TEMP_OFF 28.0f // 温度降到多少以下才算恢复正常
#define LINK_HUMI_ON 80.0f  // 湿度达到多少就开风扇（协议范围 0.0~100.0）
#define LINK_HUMI_OFF 75.0f // 湿度降到多少以下才算恢复正常

// 条件恢复正常后，再持续这么久才真的关风扇，
// 避免传感器偶尔跳一个数就让风扇跟着启停一次。
#define LINK_FAN_HOLD_MS 10000UL

// ================ 协议层开关（定义见 mqtt_proto.h） ================
// 本板动作是瞬时的（一个 ledcWrite 就完成），收到命令立即回执。
#define ACK_AFTER_DONE 0   // 无长动作，立即回执
#define SUB_EXTRA_REPORT 1 // 联动旁听：接收 4 号板的温湿度数据
#define SUB_EXTRA_ONLINE 0 // 不需要旁听上下线

// 【关于订阅 report】协议 §8.4 一般约定各板不订阅 report 以避免自回环。
// 本板为了联动有意偏离这一条，自回环已由协议层彻底堵死：
// mqtt_proto.cpp 的回调第一件事就是丢掉 src 等于本板板号的报文，
// 本板永远不会处理自己发出去的 ack。
// 把上面这个宏改成 0 即可退回成只订阅 cmd 的普通执行器。

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
