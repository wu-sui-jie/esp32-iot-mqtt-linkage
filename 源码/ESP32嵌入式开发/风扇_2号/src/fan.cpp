#include "fan.h"

#if HAS_FAN

// ============================================================
//  2 号板：继电器（风扇）模块
//
//  只做一件事：把"开 / 关"变成一次 ledcWrite。
//  继电器是开关型负载，不需要调速，所以 on 直接写满占空比 255、
//  off 写 0，中间不设档位——协议表 9 只定义了开与关两种状态。
//
//  上电默认停止。这一点是刻意的：如果上电就转，插拔电源时
//  继电器会"啪"地吸合一下，演示时容易误解成设备自己启动了。
// ============================================================

static bool fan_on = false;

void fan_init()
{
    ledcSetup(FAN_CHANNEL, FAN_FREQ, FAN_RESOLUTION);
    ledcAttachPin(FAN_PIN, FAN_CHANNEL);
    ledcWrite(FAN_CHANNEL, 0);

    fan_on = false;
    Serial.println("[fan] 初始化完成，风扇处于停止状态");
}

void fan_set_power(bool on)
{
    ledcWrite(FAN_CHANNEL, on ? 255 : 0);
    fan_on = on;

    Serial.printf("[fan] 风扇 %s\n", on ? "开（全速）" : "关");
}

bool fan_is_on()
{
    return fan_on;
}

#endif
