#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================================
//  7 号板：本地联动播报
//
//  订阅 report 与 online，把其他板的上报翻译成人话播出来，
//  不经过平台。规则表、阈值、冷却时间见 readme 第一节与本文件。
// ============================================================

// 收到非 cmd 报文（evt / dat / sys）时的联动判断入口。
// 调用链：mqtt_proto.cpp 的回调 → proto_on_other()（弱符号）
//        → linkage.cpp 里的强定义 → 本函数。
void linkage_on_message(const char *type, const char *src, JsonObjectConst body);
