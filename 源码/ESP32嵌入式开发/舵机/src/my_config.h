#pragma once

// ============================================================
//  1 号板：舵机模块 servo
//
//  板号严格按协议表 7：1 号板 = 舵机。
//  功能与设计思路见同级目录的 readme.txt 第一节与第四节。
// ============================================================

// ================ 固定的配置（所有设备都相同） ================
// 【注意】这里叫 WIFI_SSID / WIFI_PASSWORD，不叫协议附录 A 里写的
// SSID / PASSWORD。原因是宏名会全局替换：ESP32 的 WiFi.h 里有一个
// 方法就叫 SSID()，一旦 #define SSID "..."，那行声明会被替换成
//     String "IOT-SZCS01"() const;
// 直接编译报错 "expected unqualified-id before string constant"。
// 课堂例程没踩到，是因为它把 #include <WiFi.h> 写在了
// #include "my_config.h" 前面；而本工程的 mqtt_proto.h 是先包含
// my_config.h 的，所以必须改名，不能靠包含顺序。
#define WIFI_SSID "wusuijie"       // WiFi账号
#define WIFI_PASSWORD "11111111"   // WiFi密码
#define MQTT_SERVER   "yunyismart.tech" // MQTT服务器地址
#define MQTT_PORT     1883                // MQTT服务器端口号

// ================ 本组编号与主题定义 ================
// 前缀 = 课程空间前缀 202609_PP2 + 组号 1
#define GROUP_ID 1
#define TOPIC_CMD "202609_PP2/1/cmd"       // 下行：各平台 → 各板（本板只订阅这个）
#define TOPIC_REPORT "202609_PP2/1/report" // 上行：各板 → 各平台
#define TOPIC_ONLINE "202609_PP2/1/online" // 上行：上线下线，Retain

// ================ 本设备板号（1~8，按协议表 7） ================
#define BOARD_ID 1
#define BOARD_ID_STR "1"

// ================ 设备标识（协议表 7） ================
#define DEV_SERVO "servo"

// ================ 舵机硬件参数 ================
// 信号线接 GPIO14（课堂例程 4-舵机控制 的接法）。
// 若实验箱实际接线不同，只改这一行，其余代码不用动。
#define SERVO_PIN 14

// 板载 LED1：转动期间点亮，低电平点亮（课堂例程的接法）
#define PIN_LED1 2

// 协议允许的角度范围（协议表 5：0~180 的整数，单位度）
#define SERVO_ANGLE_MIN 0
#define SERVO_ANGLE_MAX 180

// 上电自动回到的中位角度
#define SERVO_HOME_DEG 90

// ================ 平滑转动参数 ================
// 转动被拆成「每 SERVO_STEP_MS 毫秒走 SERVO_STEP_DEG 度」的轨迹：
// 全程耗时 = 角度差 / 步长 × 步间隔，0° → 180° 约 1.8 秒。
// 调大步长或调小步间隔会转得更快也更冲，演示前按需改这两个宏。
#define SERVO_STEP_DEG 1 // 每步走多少度
#define SERVO_STEP_MS 10 // 每步间隔多少毫秒

// ================ 对射联动的参数 ================
// 3 号板检测到光束被遮挡时，本板自动把舵机转过去；光束恢复后转回原位。
// 这是本模块的个性化功能：不经过平台，板子自己完成判断与动作，
// 所以现场演示时响应更快，平台或网络出状况也照样能动。
#define LINK_SERVO_BLOCKED_DEG 0    // 遮挡时转到（0°）
#define LINK_SERVO_CLEAR_DEG 90     // 恢复时回到（中位 90°）

// ================ 协议层开关（定义见 mqtt_proto.h） ================
#define ACK_AFTER_DONE 1   // 转动到位之后才回执（协议表 19）
#define SUB_EXTRA_REPORT 1 // 联动旁听：接收 3 号板的遮挡事件
#define SUB_EXTRA_ONLINE 0 // 本板不订阅 online

// 【关于订阅 report】协议 §8.4 一般约定各板不订阅 report 以避免自回环。
// 本板为了联动有意偏离这一条，自回环已由协议层彻底堵死：
// mqtt_proto.cpp 的回调第一件事就是丢掉 src 等于本板板号的报文，
// 所以本板永远不会处理自己发出去的 ack。
// 把上面这个宏改成 0 即可退回成只订阅 cmd 的普通执行器。

// ================ MQTT 接收缓冲区 ================
// arduino-mqtt 默认只有 128 字节，协议里带 param 的报文可能超过，
// 必须显式放大，否则长报文会被截断。
#define MQTT_BUF_SIZE 1024
