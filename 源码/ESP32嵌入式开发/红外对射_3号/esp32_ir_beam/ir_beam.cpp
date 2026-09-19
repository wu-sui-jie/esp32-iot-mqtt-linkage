// ==========================================================
//  3 号板 · 红外对射传感器（ir_beam）
//  文件：ir_beam.cpp
//
//  判断光束有没有被挡住，状态一变就上报。
//
//  三处工程化处理缺一个现场都不好用：
//    · 去抖 50ms   —— 手抖、飞虫、气流都会让接收管瞬间误判
//    · 限流 500ms  —— 遮挡时接收管会间歇恢复，不限制会刷屏
//    · 30 秒心跳   —— 状态不变时也要定期说话，否则平台分不清
//                     "没变化"和"这块板掉线了"
//
//  参数依据协议表 19，上报格式见协议表 10，累计次数见协议表 20。
// ==========================================================

#include "ir_beam.h"
#include "mqtt_proto.h"

#if HAS_IR_BEAM

// ============================================================
//  3 号板：红外对射传感器模块
//
//  传感器只做一件事：判断光束有没有被挡住，状态一变就上报。
//  三处工程化处理缺一个现场都不好用：
//    · 去抖 50 ms —— 手抖、飞虫、气流都会让接收管瞬间误判
//    · 限流 500 ms —— 遮挡时接收管会间歇性恢复，不限制会刷屏
//    · 30 秒心跳 —— 状态一直不变时也要定期说话，否则平台
//      无法区分"状态没变化"和"这块板掉线了"
//  参数依据协议表 19。
//
//  全部判断用时间戳做非阻塞比较，没有任何 delay()：
//  主循环里跑着 proto_loop()，卡住会收不到命令、甚至被服务器踢下线。
// ============================================================

// ---------------- 状态 ----------------
static bool raw_blocked = false;       // 引脚的即时状态
static bool confirmed_blocked = false; // 去抖确认后的状态
static bool was_connected = false;     // 上一轮的 MQTT 连接状态（用于识别重连）

static unsigned long raw_change_ms = 0;   // 引脚状态最近一次变化的时刻（去抖计时起点）
static unsigned long last_blocked_ms = 0; // 最近一次上报 blocked 的时刻（限流）
static unsigned long last_clear_ms = 0;   // 最近一次上报 clear 的时刻（限流）
static unsigned long last_beat_ms = 0;    // 最近一次心跳的时刻
static unsigned long blocked_count = 0;   // 累计遮挡次数（协议表 20）

// ============================================================
//  读引脚。HIGH / LOW 哪种表示遮挡由 my_config.h 的宏决定，
//  用 #if 而不是运行时判断，免得每次读引脚都过一次分支。
// ============================================================
static bool read_pin()
{
#if IR_ACTIVE_HIGH
    return digitalRead(IR_PIN) == HIGH;
#else
    return digitalRead(IR_PIN) == LOW;
#endif
}

// ============================================================
//  上报当前确认状态（evt）
//  协议表 10：blocked 等级 2（警告），clear 等级 0（恢复）
// ============================================================
static void report_state()
{
    if (confirmed_blocked)
        proto_send_evt(DEV_IR_BEAM, "blocked", 2);
    else
        proto_send_evt(DEV_IR_BEAM, "clear", 0);
}

// ============================================================
//  上报累计遮挡次数（dat，协议表 20 扩展数据点）
// ============================================================
static void report_count()
{
    JsonArray samples = proto_dat_samples();
    JsonObject o = samples.createNestedObject();
    o["key"] = "count";
    o["value"] = blocked_count;

    proto_send_dat(DEV_IR_BEAM);
}

// ============================================================
//  对外接口
// ============================================================
void ir_beam_init()
{
    pinMode(IR_PIN, INPUT);

    raw_blocked = confirmed_blocked = read_pin();
    last_beat_ms = millis();

    Serial.printf("[ir_beam] 初始化完成，初始状态：%s\n",
                  confirmed_blocked ? "遮挡" : "通畅");
}

// 状态跟踪、去抖、限流、心跳、重连补报，全在这里
void ir_beam_loop()
{
    unsigned long now = millis();

    // ---- MQTT 刚连上（含断线重连）：补报一次当前状态 ----
    // 不补报的话，平台在重连后要等下一次状态变化或 30 秒心跳
    // 才能知道光束现在的状态，中间这段时间界面上是空的。
    bool connected = proto_connected();
    if (connected && !was_connected)
    {
        last_beat_ms = now; // 心跳从重连成功重新起算
        report_state();
        Serial.println("[ir_beam] MQTT 已连接，补报当前状态");
    }
    was_connected = connected;

    // ---- 跟踪引脚，记录变化时刻（作为去抖计时的起点）----
    bool raw = read_pin();
    if (raw != raw_blocked)
    {
        raw_blocked = raw;
        raw_change_ms = now;
    }

    // ---- 状态确认：连续稳定 IR_DEBOUNCE_MS 才认 ----
    if (raw_blocked != confirmed_blocked && now - raw_change_ms >= IR_DEBOUNCE_MS)
    {
        // 同一状态在 IR_MIN_GAP_MS 内不重复上报（限流）
        unsigned long &last_at = raw_blocked ? last_blocked_ms : last_clear_ms;
        if (now - last_at >= IR_MIN_GAP_MS)
        {
            confirmed_blocked = raw_blocked;
            last_at = now;
            last_beat_ms = now; // 状态确认后重开心跳周期，避免紧接着又发一条心跳

            Serial.printf("[ir_beam] %s\n", confirmed_blocked ? "遮挡" : "通畅");
            report_state();

            if (confirmed_blocked)
            {
                blocked_count++; // 协议表 20：每次确认遮挡后加 1
                report_count();
            }
        }
    }

    // ---- 心跳：状态持续不变时定期重发当前状态 ----
    if (now - last_beat_ms >= IR_HEARTBEAT_MS)
    {
        last_beat_ms = now;
        report_state();
    }
}

// 去抖确认后的当前状态：true = 光束被遮挡
bool ir_beam_blocked()
{
    return confirmed_blocked;
}

// 累计遮挡次数（协议表 20 扩展数据点）
unsigned long ir_beam_count()
{
    return blocked_count;
}

#endif
