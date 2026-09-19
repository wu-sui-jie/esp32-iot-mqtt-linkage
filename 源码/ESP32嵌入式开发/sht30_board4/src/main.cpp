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

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_SHT30
    sht30_init();
#endif
}

void loop()
{
    proto_loop();

#if HAS_SHT30
    sht30_loop();
#endif
}
