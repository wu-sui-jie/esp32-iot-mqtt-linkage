#pragma once

// ============================================================
//  5 号板：ADC / RGB / 蜂鸣器综合模块
//
//  板号严格按协议表 7：5 号板 = adc、rgb、buzzer 三个设备，
//  通过 body.device 区分，三者互不影响。
//  本次实现 rgb 与 buzzer，adc（电位器）暂不实现。
//
//  本板还承担两条板端联动（协议表 20 的扩展用法）：
//    · 8 号板光照过暗  → RGB 亮暖白光作照明
//    · 6 号板检测到火焰 → RGB 多彩循环 + 蜂鸣器间歇报警
//  两个场景抢同一组灯，仲裁规则见 linkage.cpp 顶部。
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
#define TOPIC_CMD "202609_PP2/1/cmd"       // 下行：各平台 → 各板
#define TOPIC_REPORT "202609_PP2/1/report" // 上行：各板 → 各平台（本板旁听）
#define TOPIC_ONLINE "202609_PP2/1/online" // 上行：上线下线，Retain

// ================ 本设备板号（1~8，按协议表 7） ================
#define BOARD_ID 5
#define BOARD_ID_STR "5"

// ================ 外设开关 ================
// 用不到的把 1 改成 0，省 ROM
#define HAS_RGB 1
#define HAS_BUZZER 1
// 电位器本次不实现。原理图确认它接在 IO35 上（输入专用脚，
// 只能做 ADC 不能做输出），将来要做的话按协议表 12 上报
// {"key":"raw","value":0~4095}，每 1 秒一次。
#define HAS_ADC 0

// ================ 设备标识（协议表 7） ================
#define DEV_RGB "rgb"
#define DEV_BUZZER "buzzer"
#define DEV_ADC "adc"

// ================ RGB 彩灯的硬件参数 ================
// 已对照原理图《SCH_RGB蜂鸣器电位器_2025-09-23》确认：
//   XL-5050RGBC-WS2812B × 4 (LED1~LED4)，数据线接 IO27。
#if HAS_RGB
#define RGB_PIN 27
#define RGB_COUNT 4
#endif

// ================ 蜂鸣器的硬件参数 ================
// 已对照原理图《SCH_RGB蜂鸣器电位器_2025-09-23》确认：
//   IO14 → R3(1kΩ) → Q1(S8050) 基极，R4(4.7kΩ) 下拉，D1(LL4148) 续流，
//   Q1 集电极驱动 BUZZER1（5V 供电）。
//   即：IO14 输出高电平 → 三极管导通 → 蜂鸣器响。
// 对照方法：模块板上的 IOxx 经 CN2 磁吸接口直连底板 H2 的 GPIOxx，
// 底板原理图 SCH_ESP32-S功能模块 里两者的引脚名一一对应。
#if HAS_BUZZER
#define BUZZER_PIN 14 // 原理图确认

// 原理图的蜂鸣器标注为 "BUZZER1 3kHz"，即无源蜂鸣器（3 kHz 是它的
// 谐振频率），必须用方波驱动，所以这里用 PWM。
// 万一实际装的是有源蜂鸣器（给电平就响），PWM 方波也能让它发声，
// 只是音色略有差异；如果听起来发闷或音量很小，把这里改成 0 改用
// 电平驱动。两种方式都由 buzzer.cpp 支持。
#define BUZZER_USE_PWM 1
#define BUZZER_ACTIVE_LEVEL 1 // 电平驱动时的有效电平（1 = 高电平响）

#define BUZZER_FREQ 3000    // 方波频率，取原理图标注的谐振频率 3 kHz
#define BUZZER_RESOLUTION 8 // LEDC 分辨率
#define BUZZER_CHANNEL 1    // LEDC 通道（与 RGB 等错开）
#endif

// ================ 火焰报警的声光参数 ================
#define FIRE_STEP_MS 300UL     // 多彩循环每色停留时长
#define FIRE_BEEP_ON_MS 300UL  // 间歇报警：响多久
#define FIRE_BEEP_OFF_MS 300UL // 间歇报警：停多久

// ================ 光照联动的参数 ================
// 两个阈值必须分开（迟滞）：只用一个阈值会在临界点反复开关灯。
#define LIGHT_DARK_LUX 50.0f    // 低于它就开灯（协议范围 0.0~65535.0）
#define LIGHT_BRIGHT_LUX 120.0f // 高于它才关灯

// 照明用的暖白色（一种颜色，不是循环）
#define LIGHT_SCENE_R 255
#define LIGHT_SCENE_G 200
#define LIGHT_SCENE_B 120

// ================ 协议层开关（定义见 mqtt_proto.h） ================
#define ACK_AFTER_DONE 0   // RGB 设色与蜂鸣器开关都是瞬时的，立即回执
#define SUB_EXTRA_REPORT 1 // 联动旁听：光照与火焰都从 report 主题来
#define SUB_EXTRA_ONLINE 0 // 不需要

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节；本板要收其他板的 dat / evt 报文，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
