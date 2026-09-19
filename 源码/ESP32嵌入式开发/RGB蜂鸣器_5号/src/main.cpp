#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "rgb.h"
#include "buzzer.h"
#include "linkage.h"

// ============================================================
//  5 号板：ADC / RGB / 蜂鸣器综合模块
//
//  本板按协议表 7 挂三个设备，本次实现 rgb 与 buzzer：
//    rgb     收 set_color{r,g,b}，四颗 WS2812 变成该颜色
//    buzzer  收 set_power{on}（持续鸣叫）与 beep{duration}（定时鸣叫）
//  外加两条本地联动，见 linkage.cpp。
//
//  收发路径有三条，最终都落到同一组硬件上：
//    平台命令   handle_device_cmd()  → rgb_apply / buzzer_*
//    光照联动   linkage.cpp          → rgb_apply
//    火焰联动   linkage.cpp          → rgb_set_fire + buzzer_set_alarm
//  谁最后生效由 linkage.cpp 的优先级决定，平台命令优先。
// ============================================================

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
//  从报文里读开关量（协议表 5：布尔值，不加引号）
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

// ============================================================
//  从报文里读鸣叫时长（协议表 5：50~5000 的整数，单位毫秒）
// ============================================================
static bool read_duration(JsonObjectConst param, unsigned int *out)
{
    if (!param.containsKey("duration"))
        return false;

    JsonVariantConst v = param["duration"];
    // 只接受数值：字符串 "500" 不算（协议表 5 要求原生数值）
    if (v.is<bool>() || (!v.is<int>() && !v.is<float>()))
        return false;

    float f = v.as<float>();
    if (f != (float)(int)f)
        return false; // 非整数，例如 500.5
    if (f < 50.0f || f > 5000.0f)
        return false; // 越界

    *out = (unsigned int)f;
    return true;
}

// ============================================================
//  协议层把下发给本板的命令送到这里
// ============================================================
ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    // ---------------- rgb ----------------
    if (strcmp(device, DEV_RGB) == 0)
    {
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

            // 平台的手动命令优先于联动：暂停联动的灯光控制，
            // 直到下一次火焰或光照状态发生变化。
            linkage_manual_takeover();
            rgb_apply((uint8_t)r, (uint8_t)g, (uint8_t)b);

            // 回发执行后的实际状态（协议表 12：param 回发当前颜色）
            ackParam["r"] = r;
            ackParam["g"] = g;
            ackParam["b"] = b;
            return PROTO_OK;
        }
#endif
        return PROTO_IGNORE; // 不认识的动作
    }

    // ---------------- buzzer ----------------
    if (strcmp(device, DEV_BUZZER) == 0)
    {
#if HAS_BUZZER
        if (strcmp(action, "set_power") == 0)
        {
            bool on;
            if (!read_switch(param, "on", &on))
            {
                Serial.println("[buzzer] set_power 参数非法（缺 on / 不是布尔值）");
                return PROTO_FAIL;
            }

            linkage_manual_takeover();
            buzzer_set_power(on);

            ackParam["on"] = on; // 协议表 12：param 回发当前状态
            return PROTO_OK;
        }

        if (strcmp(action, "beep") == 0)
        {
            unsigned int duration;
            if (!read_duration(param, &duration))
            {
                Serial.println("[buzzer] beep 参数非法（缺 duration / 非整数 / 越界 50~5000）");
                return PROTO_FAIL;
            }

            linkage_manual_takeover();
            buzzer_beep(duration);

            ackParam["duration"] = duration; // 协议表 20 的扩展动作，回发实际时长
            return PROTO_OK;
        }
#endif
        return PROTO_IGNORE;
    }

    return PROTO_IGNORE; // 不是本板实现的设备（adc 本次不做）
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）

#if HAS_RGB
    rgb_init();
#endif
#if HAS_BUZZER
    buzzer_init();
#endif
}

void loop()
{
    proto_loop();    // MQTT 收发 + 回执
    rgb_loop();      // 火焰循环动画的按帧推进
    buzzer_loop();   // 定时鸣叫的停止、间歇报警的节拍
    linkage_loop();  // 联动状态变化的执行与上报
}
