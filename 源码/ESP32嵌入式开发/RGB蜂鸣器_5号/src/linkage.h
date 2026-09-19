// ==========================================================
//  5 号板 · ADC/RGB/蜂鸣器（rgb、buzzer）
//  文件：linkage.h
//
//  本地联动的对外接口：报文入口与主循环驱动。
//  实现见 linkage.cpp。
// ==========================================================

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================================
//  5 号板：本地联动
//
//  旁听 report 主题，把其他板的传感器状态变成灯光与声音。
//  这是本板区别于普通执行器的地方：不只等平台下命令，
//  自己就能对现场情况作出反应，不依赖平台与网络。
//
//  详见 linkage.cpp 顶部的规则表与仲裁说明。
// ============================================================

// 收到非 cmd 报文（evt / dat / sys）时的联动判断入口。
// 调用链：mqtt_proto.cpp 的回调 → proto_on_other()（弱符号）
//        → linkage.cpp 里的强定义 → 本函数。
void linkage_on_message(const char *type, const char *src, JsonObjectConst body);

// 主循环里每次都要调用：把回调里记下的状态变化变成实际动作与上报。
// 【为什么动作不直接在回调里做】arduino-mqtt 规定回调里不能
// publish（会死锁），而联动动作要发 evt 上报，所以统一挪到主循环。
void linkage_loop();

// 平台下发了手动命令（set_color / buzzer）：暂时让出控制权，
// 直到下一次传感器状态发生变化时才恢复联动。
void linkage_manual_takeover();
