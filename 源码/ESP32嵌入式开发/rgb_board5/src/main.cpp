#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "rgb.h"

// ============================================================
//  从报文里读一路颜色通道
//
//  要求：这个键必须存在，且必须是数值（不能是字符串、布尔）；
//        取值必须在 0~255 之间。
//  不满足返回 false —— 不能"缺了就按 0 处理"，
//  那样发来一个残缺报文会把灯误熄灭，比直接报错更糟。
// ============================================================
static bool read_channel(JsonObjectConst param, const char *key, int *out)
{
    if (!param.containsKey(key))
        return false;

    JsonVariantConst v = param[key];
    if (!v.is<int>() && !v.is<float>())
        return false;

    float f = v.as<float>();
    if (f < 0.0f || f > 255.0f)
        return false;

    *out = (int)(f + 0.5f);
    return true;
}

// ============================================================
//  5 号板：RGB 灯
//
//  本板按协议还挂有 adc 与 buzzer，本次不实现（见 my_config.h）。
//  因此这里只处理 rgb 一个设备。
// ============================================================
ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    if (strcmp(device, DEV_RGB) != 0)
        return PROTO_IGNORE; // 不是本板实现的设备

#if HAS_RGB
    if (strcmp(action, "set_color") == 0)
    {
        int r, g, b;
        if (!read_channel(param, "r", &r) ||
            !read_channel(param, "g", &g) ||
            !read_channel(param, "b", &b))
        {
            Serial.println("[rgb] set_color 参数非法（缺通道 / 非数值 / 越界 0~255）");
            return PROTO_FAIL; // 回 ack result:"fail"，灯保持原状态
        }

        rgb_apply((uint8_t)r, (uint8_t)g, (uint8_t)b);

        // 回发执行后的实际状态，供平台确认（协议表 4：param 回发实际状态）
        ackParam["r"] = r;
        ackParam["g"] = g;
        ackParam["b"] = b;
        return PROTO_OK;
    }
#endif

    return PROTO_IGNORE; // 不认识的动作
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_RGB
    rgb_init();
#endif
}

void loop()
{
    proto_loop();
    // rgb 只做纯色，没有需要按帧刷新的东西，所以没有 rgb_loop()
}
