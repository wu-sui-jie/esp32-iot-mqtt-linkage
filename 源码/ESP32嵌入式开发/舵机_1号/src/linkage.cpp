// ==========================================================
//  1 号板 · 舵机模块（servo） · 本地联动
//  文件：linkage.cpp
//
//  旁听 report 主题，3 号板的光束遮挡事件一到，本板就把舵机转过去，
//  不经过平台：响应更快，平台或网络出状况也照样能动。
//
//  规则表（改动请同步 readme）：
//    3 号 ir_beam  evt blocked  →  舵机转到 LINK_SERVO_BLOCKED_DEG
//    3 号 ir_beam  evt clear    →  舵机回到 LINK_SERVO_CLEAR_DEG
//
//  只认状态跳变：3 号板每 30 秒的心跳 evt 不会让舵机重复转动。
// ==========================================================

#include "linkage.h"
#include "my_config.h"
#include "mqtt_proto.h"
#include "servo.h"

// ============================================================
//  1 号板：本地联动
//
//  规则表（改动请同步 readme）：
//    触发源              条件                    动作
//    ──────────────────────────────────────────────────────────
//    3 号 ir_beam   evt blocked（等级 2）   舵机转到 LINK_SERVO_BLOCKED_DEG
//    3 号 ir_beam   evt clear（等级 0）     舵机回到 LINK_SERVO_CLEAR_DEG
//
//  【联动监听的设备名】（协议表 7 里 3 号板的 device）
//  写成宏而不是散落在判断语句里，改名时只改这一处。
// ============================================================
#define LINK_DEV_IR_BEAM "ir_beam"

// ---------------- 联动状态 ----------------
static bool ir_blocked = false;   // 3 号板确认的光束状态
static bool need_update = false;  // 状态变了，待应用

// ============================================================
//  evt：对射遮挡（3 号板）
//
//  只认"状态跳变"：3 号板在状态不变时每 30 秒发一次心跳 evt
//  （协议 5.3 节的要求，防止平台以为传感器掉线），
//  那些心跳与触发无关，重复置位在这里是无害的——
//  下面的 if 保证同一个状态只会产生一次 need_update。
// ============================================================
static void on_evt(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    const char *ev = body["event"] | "";

    if (strcmp(device, LINK_DEV_IR_BEAM) != 0)
        return;

    if (strcmp(ev, "blocked") == 0)
    {
        if (!ir_blocked)
        {
            ir_blocked = true;
            need_update = true;
            Serial.println("[link] 3 号板光束被遮挡");
        }
    }
    else if (strcmp(ev, "clear") == 0)
    {
        if (ir_blocked)
        {
            ir_blocked = false;
            need_update = true;
            Serial.println("[link] 3 号板光束恢复");
        }
    }
}

// ============================================================
//  入口
// ============================================================
void linkage_on_message(const char *type, const char *src, JsonObjectConst body)
{
    (void)src; // 自己的报文已被协议层按 src 过滤，这里收到的都是别人的

    if (strcmp(type, "evt") == 0)
        on_evt(body);
}

// 状态变了就驱动舵机，并广播一条联动事件
void linkage_loop()
{
    if (!need_update)
        return;

    // 舵机还在转就等它停稳，不要中途改目标。
    // 遮挡事件本身已被 3 号板的 50ms 去抖 + 500ms 限流挡住，
    // 不会很密集，所以这里只需要避免自己打断自己。
    if (servo_busy())
        return;

    need_update = false;

    int want = ir_blocked ? LINK_SERVO_BLOCKED_DEG : LINK_SERVO_CLEAR_DEG;

    // 已经在目标位置：既不再转一次，也不再上报一条 evt，
    // 免得平台看到一串没有实际动作的 auto_move。
    if (servo_angle() == want)
        return;

    servo_set_angle(want);

    // 把联动动作广播出去：平台能看到是"谁让它转的"，
    // 7 号板收到后也会播报一句。
    proto_send_evt(DEV_SERVO, ir_blocked ? "auto_move" : "auto_home",
                   ir_blocked ? 1 : 0);

    Serial.printf("[link] 联动转动到 %d°\n", want);
}

// ============================================================
//  覆盖协议层 mqtt_proto.cpp 里的弱函数
//
//  本板订阅了 report，其他板的报文从这里进来。
// ============================================================
void proto_on_other(const char *type, const char *src, JsonObjectConst body)
{
    linkage_on_message(type, src, body);
}
