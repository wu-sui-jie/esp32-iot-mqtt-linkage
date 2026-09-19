// ==========================================================
//  协议公共层 · 接口声明（八块板共用同一份）
//  文件：mqtt_proto.h
//
//  按《MQTT 通信协议规范_第1组》V2.0 实现「信封 + 载荷」两层报文的
//  构造与解析。对外提供的接口：
//    proto_init / proto_loop / proto_connected   生命周期
//    proto_send_dat / proto_send_evt             上行数据与事件
//    handle_device_cmd                           由各板 main.cpp 实现
//    device_busy / proto_on_other                可选钩子（弱符号）
//
//  【八份副本必须逐字节一致】改一份要同步其余七份。
//  本文件不依赖任何 MQTT 库，库相关的调用都关在 mqtt_proto.cpp 里。
// ==========================================================

#pragma once

// ============================================================
//  协议公共层
//
//  按《MQTT 通信协议规范_第1组》V2.0 实现「信封 + 载荷」两层的
//  构造与解析。本组每个工程各有一份【完全相同】的副本，
//  改一份必须同步其余全部，同步后这样确认一致（在
//  ESP32嵌入式开发/ 目录下执行）：
//      for d in */; do diff -q "$d/src/mqtt_proto.cpp" 舵机/src/mqtt_proto.cpp; done
//
//  本文件不依赖具体的 MQTT 库，所有库相关的调用都关在
//  mqtt_proto.cpp 里。将来若要换库，只改那一个文件。
//
//  ── 各板之间的差异全部用宏控制，宏写在各自的 my_config.h 里 ──
//    ACK_AFTER_DONE    1 = 执行器做完动作才回执（1 号板舵机用）
//    SUB_EXTRA_REPORT  1 = 额外订阅 report（7 号板联动旁听用）
//    SUB_EXTRA_ONLINE  1 = 额外订阅 online（7 号板联动旁听用）
//  这三个宏在没有定义的工程里按 0 处理，所以本文件可以原样复制到
//  其他板的工程里，不定义这三个宏的板行为与复制前完全一致。
// ============================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include "my_config.h"

// ================ 协议常量 ================
#define PROTO_VER "1.0" // 协议版本，本版本固定
#define SEQ_MAX 65535   // seq 上限。协议表 2 只说"到达上限后回到 1"，
                        // 没写上限是多少，本组约定 65535

// ================ 命令处理结果 ================
enum ProtoResult
{
    PROTO_IGNORE = 0, // 不是本板的设备、或不认识的动作 → 不回执
    PROTO_OK,         // 执行成功 → 回 ack，result = "ok"
    PROTO_FAIL        // 参数非法或执行失败 → 回 ack，result = "fail"
};

// ============================================================
//  由各板的 main.cpp 实现：把命令分发给本板的设备模块
//
//  参数
//    device/action  报文里 body.device 与 body.action
//    param          报文里 body.param（可能为空对象，协议允许省略）
//    ackParam       若需要回发实际状态，往这里写字段，
//                   写完会被放进 ack 的 param 里一起发出
//
//  返回   PROTO_OK / PROTO_FAIL / PROTO_IGNORE
// ============================================================
ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam);

// ============================================================
//  两个可选钩子（弱符号，不实现就用默认行为）
// ============================================================

// 本板执行器是否还在动作。
// 配合 ACK_AFTER_DONE=1 使用：返回 true 时协议层会把 ack 挂起，
// 等它变回 false 再把 ack 发出去（协议表 19：执行完成后立即回执）。
// 1 号板在 servo.cpp 里定义同名强函数覆盖默认实现（默认返回 false）。
bool device_busy();

// 收到非 cmd 报文（evt / dat / sys）时被调用。
// 只有订阅了 report / online 的板才会收到（7 号板的联动播报用）。
// 7 号板在 linkage.cpp 里定义同名强函数覆盖默认实现。
// 【注意】本函数在 MQTT 回调链上被调用：arduino-mqtt 规定回调里
// 不能 publish / subscribe（会死锁），所以这里只能做判断和入队。
void proto_on_other(const char *type, const char *src, JsonObjectConst body);

// ============================================================
//  生命周期
// ============================================================

// 连接 WiFi 与 MQTT（含设置遗嘱、订阅 cmd、发布 online）
// 内部会调用 Serial.begin，应在 setup() 里第一个调用
void proto_init();

// 主循环里每次都要调用：驱动 MQTT、发送待回执、驱动执行器
void proto_loop();

// 当前是否已连上 MQTT
bool proto_connected();

// ============================================================
//  周期数据上报（type = dat）
//
//  用法：
//      JsonArray s = proto_dat_samples();   // 拿到一个空的 samples 数组
//      JsonObject o = s.createNestedObject();
//      o["key"] = "temperature";
//      o["value"] = serialized(String(t, 1));
//      proto_send_dat("sht30");             // 填完再发
//
//  proto_dat_samples() 内部会把上一次的内容清掉，可以反复调用。
//  1 号板与 7 号板都是纯执行器，没有周期数据要上报，本接口备而不用。
// ============================================================
JsonArray proto_dat_samples();
bool proto_send_dat(const char *device);

// ============================================================
//  事件上报（type = evt）
//
//  用法：
//      proto_send_evt("ir_beam", "blocked", 2);
//
//  level 为事件等级 0~3（协议表 5）：0 恢复、1 提示、2 警告、3 紧急。
//  协议表 18 规定 evt 与 ack 一样用 QoS 1，不允许丢失。
//  上报周期与去抖参数见协议表 19。
// ============================================================
bool proto_send_evt(const char *device, const char *event, int level);
