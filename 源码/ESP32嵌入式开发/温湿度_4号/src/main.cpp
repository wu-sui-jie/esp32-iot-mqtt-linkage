// ==========================================================
//  4 号板 · 温湿度传感器（sht30）
//  文件：main.cpp
//
//  程序入口。setup() 里初始化协议层与各功能模块，
//  loop() 里依次驱动协议收发与各模块的状态机。
//
//  平台下发的命令由 handle_device_cmd() 分发到本板的具体模块。
// ==========================================================

#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "sht30.h"

// ============================================================
//  4 号板：温湿度传感器 SHT30
//  本板是纯传感器板，只有周期上报，没有可控设备。
//  收到任何命令都返回 PROTO_IGNORE（不回执）—— 协议要求的是
//  "设备被要求执行动作时必须回 ack"，本板没有可执行的动作。
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

// 上电初始化：先起协议层（含连网），再初始化本板的外设模块
void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_SHT30
    sht30_init();
#endif
}

// 主循环：先驱动协议收发，再依次跑各模块的状态机
void loop()
{
    proto_loop();

#if HAS_SHT30
    sht30_loop();
#endif
}
