#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "fan.h"
#include "linkage.h"

// ============================================================
//  2 号板：继电器（风扇）模块（执行器）
//
//  收 cmd → 校验 on → 开关风扇 → 回 ack。
//  本板没有周期数据要上报。
//
//  另有本地联动：4 号板温湿度超标时本板自动开风扇，见 linkage.cpp。
//
//  【迁移说明】本工程原先用 PubSubClient 写成单文件 main.cpp。
//  协议 §8.2 要求 ack 用 QoS 1，而 PubSubClient 的 publish()
//  只能发 QoS 0，所以改用 256dpi/MQTT 并拆成
//  my_config.h + mqtt_proto + fan + linkage + main 的分层结构，
//  与其余各板的命名和分层保持一致。风扇的控制逻辑未变。
// ============================================================

// ============================================================
//  从报文里读开关量
//
//  要求：这个键必须存在，且必须是布尔值（true / false，不加引号）。
//  不满足返回 false —— 不能"缺了就按 false 处理"：
//  那样收到一个残缺报文会把正在转的风扇误关掉，比直接报错更糟。
//  协议表 5 明确规定开关类参数用 JSON 原生布尔值，字符串 "true"
//  不属于合法取值，一律挡掉。
// ============================================================
static bool read_switch(JsonObjectConst param, const char *key, bool *out)
{
    if (!param.containsKey(key))
        return false;

    JsonVariantConst v = param[key];
    if (!v.is<bool>())
        return false;

    *out = v.as<bool>();
    return true;
}

ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    if (strcmp(device, DEV_FAN) != 0)
        return PROTO_IGNORE; // 不是本板实现的设备

#if HAS_FAN
    if (strcmp(action, "set_power") == 0)
    {
        bool on;
        if (!read_switch(param, "on", &on))
        {
            Serial.println("[fan] set_power 参数非法（缺 on / 不是布尔值）");
            return PROTO_FAIL; // 回 ack result:"fail"，风扇保持原状态
        }

        fan_set_power(on);

        // 回发执行后的实际状态（协议表 9：param 回发当前开关状态）
        ackParam["on"] = on;
        return PROTO_OK;
    }
#endif

    return PROTO_IGNORE; // 不认识的动作
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_FAN
    fan_init();
#endif
}

void loop()
{
    proto_loop();   // MQTT 收发 + 回执
    linkage_loop(); // 温湿度联动的开关动作与上报
    // 风扇是纯开关负载，没有需要按帧刷新的东西，所以没有 fan_loop()
}
