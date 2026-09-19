// ==========================================================
//  3 号板 · 红外对射传感器（ir_beam）
//  文件：my_config.h
//
//  本板唯一的配置文件：WiFi 与 MQTT 服务器、三条主题、板号与设备名、
//  引脚定义、各功能模块的阈值与时序参数、协议层开关，全部集中在这里。
//
//  【现场需要调整的参数都在这一个文件里】，业务代码不用动。
// ==========================================================

#pragma once

// ============================================================
//  3 号板：红外对射传感器模块 ir_beam
//
//  板号严格按协议表 7：3 号板 = 红外对射传感器。
//  本板是纯传感器板，只有周期与事件上报，没有可控设备。
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
#define BOARD_ID 3
#define BOARD_ID_STR "3"

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_IR_BEAM 1

// ================ 设备标识（协议表 7） ================
#define DEV_IR_BEAM "ir_beam"

// ================ 红外对射硬件参数 ================
// 对射模块的 DO 数字输出接 GPIO14（实验箱接法）。
// IR_ACTIVE_HIGH = 1 表示「光束被遮挡时 DO 输出高电平」，
// 若实际模块相反（遮挡时输出低电平），把这一行改成 0 即可。
#define IR_PIN 14
#define IR_ACTIVE_HIGH 1

// ================ 上报与去抖参数（协议表 19） ================
#define IR_DEBOUNCE_MS 50UL     // 状态需连续稳定 50ms 才确认（软件去抖）
#define IR_MIN_GAP_MS 500UL     // 同一状态 500ms 内不重复上报（限流）
#define IR_HEARTBEAT_MS 30000UL // 状态不变时每 30 秒重发一次（心跳）

// ================ 协议层开关（定义见 mqtt_proto.h） ================
// 本板是纯传感器板，没有执行器，也不需要旁听别人的报文。
#define ACK_AFTER_DONE 0    // 无执行器，收到命令立即回执
#define SUB_EXTRA_REPORT 0  // 不旁听 report
#define SUB_EXTRA_ONLINE 0  // 不旁听 online

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
