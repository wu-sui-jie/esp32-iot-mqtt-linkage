#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "flame.h"

// ============================================================
//  6 号板：火焰传感器模块（纯传感器）
//
//  本板只有 evt 事件与 dat 强度上报，没有可控设备。
//  收到任何命令都返回 PROTO_IGNORE（不回执）—— 协议要求的是
//  "设备被要求执行动作时必须回 ack"，本板没有可执行的动作。
//
//  【迁移说明】本工程原先用 PubSubClient 写成单文件 main.cpp。
//  协议 §8.2 要求 evt 用 QoS 1，而 PubSubClient 的 publish()
//  只能发 QoS 0，所以改用 256dpi/MQTT 并拆成
//  my_config.h + mqtt_proto + flame + main 的四层结构，
//  与其余各板的命名和分层保持一致。火焰检测逻辑未变。
// ============================================================

ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    (void)device;
    (void)action;
    (void)param;
    (void)ackParam;
    return PROTO_IGNORE;
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_FLAME
    flame_init(); // 含 2 秒基线校准，此期间请勿点火
#endif
}

void loop()
{
    proto_loop();

#if HAS_FLAME
    flame_loop();
#endif
}
