#pragma once

// ============================================================
//  6 号板：火焰传感器模块 flame
//
//  板号严格按协议表 7：6 号板 = 火焰传感器。
//  本板是纯传感器板，只有事件与强度上报，没有可控设备。
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
#define BOARD_ID 6
#define BOARD_ID_STR "6"

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_FLAME 1

// ================ 设备标识（协议表 7） ================
#define DEV_FLAME "flame"

// ================ 火焰传感器硬件参数 ================
// 本版只用电炋量 AO 判断，不依赖 DO（数字量），好处是不管模块是
// 高有效还是低有效都适用：开机先采样得到"无火时的基线"，之后只要
// AO 偏离基线超过阈值就判为有火焰。
//
// 实验中 AO 到底接在 IO33 还是 IO32 未最终确定，所以两路同时监测，
// 取偏离基线更大的那一路作为信号来源。
#define PIN_FLAME_AO_1 33 // 候选 1
#define PIN_FLAME_AO_2 32 // 候选 2
#define FLAME_ADC_BITS 12 // 12 位 ADC，读数 0~4095

// ================ 判定与上报参数 ================
#define FLAME_THRESHOLD 3000 // AO 偏离基线超过它 → 有火焰（现场可调）
#define FLAME_CALIBRATE_MS 2000UL // 开机基线校准时长（此期间请勿点火）

// 去抖与限流依据协议表 19；重发周期见协议 5.6 节
#define FLAME_DEBOUNCE_MS 50UL    // 开关量需连续稳定 50ms 才确认
#define FLAME_LIMIT_MS 500UL      // 同一触发事件 500ms 内不重复上报
#define FLAME_RESEND_MS 5000UL    // 火焰持续期间每 5 秒重发一次 detected

// 强度上报周期。协议表 19 规定火焰 dat 每 1 秒一次。
#define FLAME_DAT_PERIOD_MS 1000UL

// ================ 协议层开关（定义见 mqtt_proto.h） ================
// 本板是纯传感器板，没有执行器，也不需要旁听别人的报文。
#define ACK_AFTER_DONE 0    // 无执行器，收到命令立即回执
#define SUB_EXTRA_REPORT 0  // 不旁听 report
#define SUB_EXTRA_ONLINE 0  // 不旁听 online

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
