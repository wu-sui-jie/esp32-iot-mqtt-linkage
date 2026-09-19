#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "servo.h"

// ============================================================
//  1 号板：舵机模块（执行器）
//
//  收 cmd → 校验角度 → 启动平滑转动 → 转到位后回 ack。
//  平滑转动与回执时机在 servo.cpp 与 mqtt_proto.cpp 里，
//  本文件只负责「校验参数」和「接上协议层」。
// ============================================================

// ============================================================
//  从报文里读角度
//
//  要求：这个键必须存在，且必须是数值（字符串、布尔、null 都不行）；
//        必须正好是整数（90.5 判非法）；
//        必须在 0~180 之间。
//
//  任何一条不满足都返回 false —— 不能"缺了就按 0 处理"：
//  把 135° 误当成 0° 直接甩过去会打到限位、烧齿轮，
//  比什么都不做危险得多（详见 readme 第四节 P1）。
// ============================================================
static bool read_angle(JsonObjectConst param, int *out)
{
    if (!param.containsKey("angle"))
        return false;

    JsonVariantConst v = param["angle"];

    // 只接受数值：布尔 true 会被当成 1、字符串 "90" 会被当成 90，
    // 这两种都必须挡掉（协议表 5：角度是 JSON 原生数值，不加引号）
    if (v.is<bool>() || (!v.is<int>() && !v.is<float>()))
        return false;

    float f = v.as<float>();
    if (f != (float)(int)f)
        return false; // 非整数，例如 90.5

    if (f < (float)SERVO_ANGLE_MIN || f > (float)SERVO_ANGLE_MAX)
        return false; // 越界

    *out = (int)f;
    return true;
}

// ============================================================
//  协议层把下发给本板的命令送到这里
// ============================================================
ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    if (strcmp(device, DEV_SERVO) != 0)
        return PROTO_IGNORE; // 不是本板实现的设备

    if (strcmp(action, "set_angle") != 0)
        return PROTO_IGNORE; // 不认识的动作

    int angle;
    if (!read_angle(param, &angle))
    {
        Serial.println("[servo] set_angle 参数非法（缺 angle / 非数值 / 非整数 / 越界 0~180）");
        return PROTO_FAIL; // 回 ack result:"fail"，舵机保持原位不动
    }

    servo_set_angle(angle);

    // 回发实际角度，供平台确认（协议表 8）。
    // 【注意】这里只是把字段准备好，真正的 ack 要等舵机转到位之后才发出去，
    //         发出时机由协议层的 device_busy() 钩子控制（见 servo.cpp 末尾）。
    ackParam["angle"] = angle;
    return PROTO_OK;
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）
    servo_init(); // 接舵机、回中位、配 LED1
}

void loop()
{
    proto_loop(); // MQTT 收发 + 回执
    servo_loop(); // 平滑转动，每次只走一小步，不阻塞主循环
}
