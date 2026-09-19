#pragma once

// ============================================================
//  协议公共层
//
//  按《MQTT 通信协议规范_第1组》V2.0 实现「信封 + 载荷」两层的
//  构造与解析。两块板共用同一份源码，改一处要同步另一块板。
//
//  本文件不依赖具体的 MQTT 库，所有库相关的调用都关在
//  mqtt_proto.cpp 里。将来若要换库，只改那一个文件。
// ============================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include "my_config.h"

// ================ 协议常量 ================
#define PROTO_VER "1.0" // 协议版本，本版本固定
#define SEQ_MAX 65535   // seq 上限。协议表 2 只说"到达上限后回到 1"，
                        // 没写上限是多少，本组约定 65535（见 readme P1）

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
//  生命周期
// ============================================================

// 连接 WiFi 与 MQTT（含设置遗嘱、订阅 cmd、发布 online）
// 内部会调用 Serial.begin，应在 setup() 里第一个调用
void proto_init();

// 主循环里每次都要调用：驱动 MQTT、发送待回执
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
// ============================================================
JsonArray proto_dat_samples();
bool proto_send_dat(const char *device);
