#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================================
//  7 号板：本地联动播报
//
//  订阅 report 与 online，把其他板的运行情况翻译成人话播出来，
//  不经过平台。三层播报结构（事件即时 / 阈值告警 / 周期汇总）
//  与完整的规则表见 linkage.cpp 顶部。
// ============================================================

// 联动层的初始化。必须在 setup() 里、proto_init() 【之后】调用：
// 它记录开机静默期的起点，而静默期要从"MQTT 订阅已经建立好"那一刻
// 开始算（订阅一建立，服务器就把当前在线的板全推过来了）。
void linkage_init();

// 收到非 cmd 报文（evt / dat / sys / ack）时的联动判断入口。
// 调用链：mqtt_proto.cpp 的回调 → proto_on_other()（弱符号）
//        → linkage.cpp 里的强定义 → 本函数。
// 【注意】本函数在 MQTT 回调链上被调用：arduino-mqtt 规定回调里
// 不能 publish / subscribe（会死锁），所以这里只做判断与入队。
void linkage_on_message(const char *type, const char *src, JsonObjectConst body);

// 主循环里每次都要调用：驱动第三层的周期数据汇总播报。
// 汇总要"空闲才播"，判断当前是否空闲必须离开回调，所以放在这里。
void linkage_loop();
