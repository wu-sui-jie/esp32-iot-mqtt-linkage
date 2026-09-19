#include "linkage.h"
#include "my_config.h"
#include "tts.h"

#include <stdarg.h> // 周期汇总的文本拼接用（appendf）

// ============================================================
//  7 号板：本地联动播报
//
//  这是本模块区别于别的执行器的地方：它不只等平台下命令，还会
//  【听】其他板子在干什么，然后用自己的话把运行情况播报出来。
//
//  ══════════════ 三层播报结构（本次重新设计的核心）══════════════
//
//  第一层  事件即时播报（evt / sys / ack）        状态一变立刻播
//    ├─ 6 号 flame      evt detected   "警告，检测到火焰"      3 紧急
//    ├─ 6 号 flame      evt clear      "火焰警报已解除"        0 恢复
//    ├─ 3 号 ir_beam    evt blocked    "注意，检测到遮挡"      2 警告
//    ├─ 3 号 ir_beam    evt clear      "遮挡已恢复"            0 恢复
//    ├─ 1 号 servo      evt auto_move  "舵机已转到遮挡位"      1 提示
//    ├─ 1 号 servo      evt auto_home  "舵机已复位"            1 提示
//    ├─ 2 号 fan        evt auto_on    "环境超标，已自动开启风扇" 1 提示
//    ├─ 2 号 fan        evt auto_off   "环境恢复正常，已关闭风扇" 0 恢复
//    ├─ 5 号 rgb        evt scene_light "光线过暗，已开启照明"  1 提示
//    ├─ 任意板          sys online/offline  "X 号设备已上线/离线"
//    └─ 1 号 servo      ack ok,angle=X "舵机已转到 X 度"（平台命令）
//
//  第二层  阈值告警（dat 越界）                    带冷却，防止刷屏
//    ├─ 4 号 sht30      temperature ≥ 30.0  "温度过高，当前 XX.X 度"
//    ├─ 4 号 sht30      humidity    ≥ 80.0  "湿度偏高，当前 XX%"
//    └─ 8 号 light      illuminance < 50    "光线过暗，请注意"
//
//  第三层  周期数据汇总（dat 快照）                最低优先，空闲才播
//    └─ 每 SUMMARY_PERIOD_MS（默认 30 秒）把各模块的最新值
//       合成一句话念一遍："环境汇总，温度26.5度，湿度60%，
//       光照125勒克斯，对射通畅，火焰正常"
//
//  ── 为什么要有第三层 ──
//  前两层都是"出了事才说话"。传感器数据在正常范围内时一句都不播，
//  于是听不到系统整体的运行状况。但【逐条播报在物理上做不到】：
//  八块板合起来每秒产生两三条报文，而语音念一句话要两三秒，
//  数据产生速度是播报能力的 5~7 倍，排多长的队都会越播越滞后。
//  所以第三层不做队列，只做【快照】——每个数据源只记最新值，
//  新数据来了覆盖旧的，到点合成一句念出来。念的永远是当前值。
//
//  ── 关于场景事件的重复问题 ──
//  5 号板在进入火焰场景时会发 rgb/scene_fire 与 buzzer/alarm_on，
//  这两条【不再单独播报】：6 号板的 flame/detected 已经把"着火了"
//  这件事说清楚了，再播两遍只是吵。scene_light 则保留，
//  因为它说明的是"照明已经开了"，与 light_sensor 的光线告警
//  不是同一件事（照明可能因为平台接管或 5 号板离线而没开）。
//
//  【本文件只在 MQTT 回调链上被调用】所以这里只做「判断 + 入队」，
//  真正的串口下发在 tts_loop() 里做。arduino-mqtt 规定回调里不能
//  publish / subscribe（会死锁），慢操作也应当挪出回调。
// ============================================================

// ============================================================
//  规则表：每条规则一个独立的冷却计时槽
// ============================================================
enum Rule
{
    R_FLAME_ON = 0, // 检测到火焰
    R_FLAME_OFF,    // 火焰解除
    R_IR_ON,        // 光束遮挡
    R_IR_OFF,       // 遮挡恢复
    R_AUTO,         // 联动动作（舵机转动 / 风扇启停）
    R_LIGHT_SCENE,  // 照明开启
    R_TEMP,         // 温度过高
    R_HUMI,         // 湿度过高
    R_LIGHT,        // 光线过暗（阈值告警）
    R_ONLINE,       // 设备上线
    R_OFFLINE,      // 设备离线
    R_SERVO,        // 平台命令的舵机动作
    R_COUNT
};

static uint32_t last_fire_ms[R_COUNT] = {0};

// 等级决定默认冷却时间：紧急 / 警告用短的，提示 / 恢复用长的
static uint32_t cooldown_of(uint8_t level)
{
    return (level >= TTS_LEVEL_WARN) ? COOLDOWN_ALARM_MS : COOLDOWN_INFO_MS;
}

// 冷却检查：被挡下来的不算"播过"，时间戳不会顺延，
// 这样告警只要持续存在，冷却一到就会再播一次。
static bool cooldown_ok(Rule r, uint32_t cooldown_ms)
{
    uint32_t now = millis();
    if (last_fire_ms[r] != 0 && (now - last_fire_ms[r]) < cooldown_ms)
        return false;
    last_fire_ms[r] = now;
    return true;
}

// 与 say_rule 相同，只是冷却时间单独给。
// 返回值表示这句话有没有真的被受理（false = 被冷却挡掉或队列满了）。
static bool say_rule_cd(Rule r, const char *text, uint8_t level, uint32_t cooldown_ms)
{
    if (!cooldown_ok(r, cooldown_ms))
    {
        Serial.printf("[link] 冷却中，跳过：%s\n", text);
        return false;
    }
    return tts_say(text, level);
}

static void say_rule(Rule r, const char *text, uint8_t level)
{
    say_rule_cd(r, text, level, cooldown_of(level));
}

// ============================================================
//  数据快照（第三层的输入）
//
//  每个数据源只保留最新值，新数据覆盖旧值。
//  与"队列"的区别：队列会把每条数据都存下来等着播，
//  快照只留当前状态，所以永远不会滞后。
// ============================================================
struct Snapshot
{
    bool has_temp, has_humi, has_lux; // 是否收到过这一类数据
    float temp, humi, lux;

    bool has_ir;      // 是否收到过对射的状态
    bool ir_blocked;  // true = 遮挡

    bool has_flame;    // 是否收到过火焰的状态
    bool flame_alarm;  // true = 正在报火焰
};

static Snapshot snap;

// 开机静默期的起点。由 linkage_init() 在协议层初始化完成之后记录，
// 不能用 millis() 直接算——原因见 linkage_init() 里的说明。
static uint32_t mute_start_ms = 0;

// ============================================================
//  各板在线状态表
//
//  【为什么需要它】online 主题是 Retain 的：某块板掉线后，服务器会把
//  它的 offline 报文一直保留着，直到这块板重新上线才被覆盖。而语音板
//  每一次【重新订阅】（冷启动，或者与服务器断线重连）都会把服务器上
//  当前保留的全部 online / offline 再收一遍。
//
//  不加区分地播报就会出现倒着的现象：设备一接电，先播一句"X 号设备
//  已离线"，紧接着才播"已上线"。前一句其实是它上一次掉线时留下的
//  旧报文，不是刚发生的事。
//
//  有了这张表就能分辨：只有"表里记着它在线、现在却收到 offline"
//  才是真的掉线；表里本来就是离线（或压根没见过它），那收到的
//  offline 就是服务器上的旧报文，不播。online 同理，重复的不播第二遍。
// ============================================================
static bool board_online[9] = {false}; // 下标 1~8 对应板号，0 不用

// ============================================================
//  sys：设备上下线（发布在 online 主题，Retain）
// ============================================================
static void on_sys(const char *src, JsonObjectConst body)
{
    const char *ev = body["event"] | "";
    int board = atoi(src);

    if (board < 1 || board > 8)
    {
        Serial.printf("[link] sys 报文的 src 不是有效板号（\"%s\"），忽略\n", src);
        return;
    }

    if (strcmp(ev, "online") == 0)
    {
        bool was_online = board_online[board];
        board_online[board] = true; // 状态一定要更新，播不播是另一回事

        if (was_online)
        {
            Serial.printf("[link] %d 号板重复的 online，不播报\n", board);
            return;
        }

        // 开机静默期：冷启动订阅之后，服务器会把当前在线的板一股脑
        // 推过来，不屏蔽的话开机就是一串"X 号设备已上线"。
        // 【只挡播报，不挡上面那句状态更新】否则静默期内上线的板在表里
        // 仍然记成离线，之后它真掉线时会被当成"旧报文"而不播。
        if (millis() - mute_start_ms < ONLINE_MUTE_MS)
        {
            Serial.printf("[link] 静默期内，%d 号板上线只记录不播报\n", board);
            return;
        }

        char text[48];
        snprintf(text, sizeof(text), "%d 号设备已上线", board);
        say_rule(R_ONLINE, text, TTS_LEVEL_INFO);
    }
    else if (strcmp(ev, "offline") == 0)
    {
        if (!board_online[board])
        {
            // 表里本来就是离线：这是服务器保留的旧报文，不是刚发生的掉线
            Serial.printf("[link] %d 号板本来就是离线，"
                          "这条 offline 是服务器保留的旧报文，不播报\n",
                          board);
            return;
        }
        board_online[board] = false;

        char text[48];
        snprintf(text, sizeof(text), "%d 号设备已离线", board);
        say_rule(R_OFFLINE, text, TTS_LEVEL_WARN);
    }
}

// ============================================================
//  evt：状态事件（第一层）
//
//  火焰按【轮次】播报，不按报文条数：6 号板在火焰持续期间每 5 秒
//  重发一次 detected（防平台漏报），那些重发不是新事件。本板记住
//  "本轮已经播过告警"，同一轮里只按 FLAME_REMIND_MS 提醒，
//  收到 clear 本轮结束并重新武装，于是"灭火后重新点火"立刻就能播。
//  上一版是拿 30 秒冷却去压这些重发的，代价是演示时点火常常不响。
// ============================================================
static bool flame_alarming = false;   // 本轮火焰是否已播过告警
static uint32_t last_flame_rx_ms = 0; // 上一条 detected 的到达时刻
static uint32_t last_flame_say_ms = 0;// 上一次播"检测到火焰"的时刻

static void on_evt_flame(const char *ev)
{
    if (strcmp(ev, "detected") == 0)
    {
        uint32_t now = millis();

        // 本轮没播过（第一次点火，或中间收到过 clear），或者距上一条
        // detected 超过一个重发周期（火焰断过、只是 clear 丢了）→ 新一轮
        bool new_round = !flame_alarming ||
                         (now - last_flame_rx_ms) > FLAME_RESEND_GAP_MS;
        last_flame_rx_ms = now;

        // 同一轮里没到提醒间隔的就是重发，不出声
        if (!new_round && (now - last_flame_say_ms) < FLAME_REMIND_MS)
        {
            Serial.println("[link] 同一轮火焰的重发上报，不重播");
            return;
        }
        // 新一轮走 3 秒的抖动地板，不走 30 秒的告警冷却，
        // 这样灭火后再点火立刻就能播。只有真的播出去了才置标记：
        // 被冷却或队列挡下的那次不记账，后面的重发还有机会补播。
        if (say_rule_cd(R_FLAME_ON, "警告，检测到火焰",
                        TTS_LEVEL_EMERGENCY, COOLDOWN_FLAME_MS))
        {
            flame_alarming = true;
            last_flame_say_ms = now;
        }
    }
    else if (strcmp(ev, "clear") == 0)
    {
        // 本轮播过告警才播"已解除"，两者配对，抖动时不会来回念。
        // 这里同样走火焰自己的短冷却，免得演示时每轮只有第一次灭火听得见。
        if (flame_alarming)
        {
            flame_alarming = false;
            say_rule_cd(R_FLAME_OFF, "火焰警报已解除",
                        TTS_LEVEL_RECOVER, COOLDOWN_FLAME_MS);
        }
    }
}

static void on_evt(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    const char *ev = body["event"] | "";

    if (strcmp(device, "flame") == 0)
    {
        // 先更新快照，再走播报逻辑
        snap.has_flame = true;
        snap.flame_alarm = (strcmp(ev, "detected") == 0);
        on_evt_flame(ev);
    }
    else if (strcmp(device, "ir_beam") == 0)
    {
        snap.has_ir = true;
        snap.ir_blocked = (strcmp(ev, "blocked") == 0);

        if (strcmp(ev, "blocked") == 0)
            say_rule(R_IR_ON, "注意，检测到遮挡", TTS_LEVEL_WARN);
        else if (strcmp(ev, "clear") == 0)
            say_rule(R_IR_OFF, "遮挡已恢复", TTS_LEVEL_RECOVER);
    }
    else if (strcmp(device, "servo") == 0)
    {
        // 1 号板的板端联动动作。它与平台命令是两回事：
        // 平台命令会走 ack（下面 on_ack 处理，带具体角度），
        // 联动只发这条 evt，没有角度字段，所以按动作说一句话即可。
        if (strcmp(ev, "auto_move") == 0)
            say_rule_cd(R_AUTO, "舵机已转到遮挡位", TTS_LEVEL_INFO, COOLDOWN_AUTO_MS);
        else if (strcmp(ev, "auto_home") == 0)
            say_rule_cd(R_AUTO, "舵机已复位", TTS_LEVEL_INFO, COOLDOWN_AUTO_MS);
    }
    else if (strcmp(device, "fan") == 0)
    {
        // 2 号板的板端联动动作
        if (strcmp(ev, "auto_on") == 0)
            say_rule_cd(R_AUTO, "环境超标，已自动开启风扇", TTS_LEVEL_INFO, COOLDOWN_AUTO_MS);
        else if (strcmp(ev, "auto_off") == 0)
            say_rule_cd(R_AUTO, "环境恢复正常，已关闭风扇", TTS_LEVEL_RECOVER, COOLDOWN_AUTO_MS);
    }
    else if (strcmp(device, "rgb") == 0)
    {
        // 5 号板的照明场景。scene_fire / scene_off 不播：
        // 火焰那句告警已经说过了，buzzer 的报警也不重复播。
        if (strcmp(ev, "scene_light") == 0)
            say_rule(R_LIGHT_SCENE, "光线过暗，已开启照明", TTS_LEVEL_INFO);
    }
}

// ============================================================
//  dat：周期数据（第二层的告警 + 第三层的快照）
//
//  收到数据先更新快照（不管值是多少），再看有没有越界要告警。
//  快照更新永远发生，所以汇总播报里始终是最新值。
// ============================================================
static void on_dat(JsonObjectConst body)
{
    const char *device = body["device"] | "";

    // 一条 dat 报文可以带多个数据点，逐个看
    JsonArrayConst samples = body["samples"].as<JsonArrayConst>();
    for (JsonVariantConst s : samples)
    {
        const char *key = s["key"] | "";
        JsonVariantConst v = s["value"];
        if (!v.is<int>() && !v.is<float>())
            continue; // value 不是数值就不管
        float val = v.as<float>();

        if (strcmp(device, "sht30") == 0)
        {
            if (strcmp(key, "temperature") == 0)
            {
                snap.has_temp = true;
                snap.temp = val;

                if (val >= TEMP_HIGH)
                {
                    char text[48];
                    snprintf(text, sizeof(text), "温度过高，当前 %.1f 度", val);
                    say_rule(R_TEMP, text, TTS_LEVEL_WARN);
                }
            }
            else if (strcmp(key, "humidity") == 0)
            {
                snap.has_humi = true;
                snap.humi = val;

                if (val >= HUMI_HIGH)
                {
                    char text[48];
                    snprintf(text, sizeof(text), "湿度偏高，当前 %d%%", (int)(val + 0.5f));
                    say_rule(R_HUMI, text, TTS_LEVEL_INFO);
                }
            }
        }
        else if (strcmp(device, "light_sensor") == 0)
        {
            if (strcmp(key, "illuminance") == 0)
            {
                snap.has_lux = true;
                snap.lux = val;

                if (val < LIGHT_LOW)
                    say_rule(R_LIGHT, "光线过暗，请注意", TTS_LEVEL_INFO);
            }
        }
    }
}

// ============================================================
//  ack：平台命令的舵机动作（第一层）
//
//  协议表 8 只给 1 号板定义了 cmd 与 ack 两类报文，它没有 evt、也没有
//  dat，所以"舵机转了多少度"只能从它的 ack 里读出来。
//  1 号板的 ack 是【转到位之后】才发出来的，所以这里播报的时机正好
//  是舵机刚停稳，"舵机已转到 180 度"说出来和动作是合上的。
//
//  【注意】这只处理平台下发的命令。1 号板自己的联动转动不发 ack，
//  走的是上面的 evt auto_move / auto_home，两条路径不重叠。
// ============================================================
#if TTS_ANNOUNCE_SERVO
static int last_servo_deg = -1; // 上一次播报过的角度，用来抑制重复
#endif

static void on_ack(JsonObjectConst body)
{
#if TTS_ANNOUNCE_SERVO
    const char *device = body["device"] | "";
    const char *action = body["action"] | "";
    const char *result = body["result"] | "";

    if (strcmp(device, "servo") != 0 || strcmp(action, "set_angle") != 0)
        return;

    // 参数非法被拒绝的命令并没有让舵机动，不能播报
    if (strcmp(result, "ok") != 0)
        return;

    JsonVariantConst v = body["param"]["angle"];
    if (!v.is<int>() && !v.is<float>())
        return;
    int angle = (int)(v.as<float>() + 0.5f);

    // 同一个角度不重复播报：平台连点同一个按钮、QoS 1 重投递都不会刷屏
    if (angle == last_servo_deg)
        return;

    char text[48];
    snprintf(text, sizeof(text), "舵机已转到 %d 度", angle);
    // 只有真的播出去了才记账。被冷却挡掉的那次不记账，
    // 免得"180 之后紧接着 90、冷却把 90 挡了"时，90 这句永远补不回来。
    if (say_rule_cd(R_SERVO, text, TTS_LEVEL_INFO, COOLDOWN_SERVO_MS))
        last_servo_deg = angle;
#else
    (void)body;
#endif
}

// ============================================================
//  第三层：周期数据汇总
//
//  把各模块的最新值合成一句话播出来。空闲才播，不插队、不打断告警。
// ============================================================
#if TTS_SUMMARY_ENABLE

static uint32_t last_summary_ms = 0;

// 追加格式化。snprintf 返回的是"本应写入的长度"，
// 可能超过剩余空间，所以这里要做钳位，否则 pos 会越过缓冲区末尾。
static int appendf(char *buf, size_t n, int pos, const char *fmt, ...)
{
    if (pos >= (int)n - 1)
        return pos;

    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf + pos, n - pos, fmt, ap);
    va_end(ap);

    if (r < 0)
        return pos;

    int room = (int)(n - pos) - 1;
    return pos + (r < room ? r : room);
}

// 26.0 → "26"；26.5 → "26.5"。
// 语音模块念"二十六点零度"很别扭，整数就别带小数位。
static void fmt_num(char *buf, size_t n, float v)
{
    if (v == (float)(int)v)
        snprintf(buf, n, "%d", (int)v);
    else
        snprintf(buf, n, "%.1f", v);
}

// ============================================================
//  拼一句话
//
//  没有数据的模块【整段省略】，不念"未知"——
//  句子始终是通顺的，掉一块板也能听完整句。
//  整句控制在 100 字节（GB2312）以内，这是协议 §5.7 的建议上限。
// ============================================================
static void build_summary(char *out, size_t n)
{
    char num[16];
    int pos = 0;

    pos = appendf(out, n, pos, "环境汇总");

    if (snap.has_temp && snap.has_humi)
    {
        fmt_num(num, sizeof(num), snap.temp);
        pos = appendf(out, n, pos, "，温度%s度，湿度%d%%",
                      num, (int)(snap.humi + 0.5f));
    }
    else if (snap.has_temp)
    {
        fmt_num(num, sizeof(num), snap.temp);
        pos = appendf(out, n, pos, "，温度%s度", num);
    }
    else if (snap.has_humi)
    {
        pos = appendf(out, n, pos, "，湿度%d%%", (int)(snap.humi + 0.5f));
    }

    if (snap.has_lux)
        pos = appendf(out, n, pos, "，光照%d勒克斯", (int)(snap.lux + 0.5f));

    if (snap.has_ir)
        pos = appendf(out, n, pos, "，对射%s", snap.ir_blocked ? "遮挡" : "通畅");

    if (snap.has_flame)
        pos = appendf(out, n, pos, "，火焰%s", snap.flame_alarm ? "报警" : "正常");

    out[n - 1] = '\0';
}

static void summary_tick()
{
    if (millis() - last_summary_ms < SUMMARY_PERIOD_MS)
        return;

    // 一条数据都没收到就先不播（刚上电时其他板还没起来）
    if (!snap.has_temp && !snap.has_humi && !snap.has_lux && !snap.has_ir && !snap.has_flame)
    {
        last_summary_ms = millis(); // 等下一个周期再看
        return;
    }

    // 最低优先级：有告警在播、或有告警在排队，这一轮就让路。
    // 【注意这里不更新时间戳】所以下一轮主循环还会来问，
    // 一空闲立刻补上，不会因为忙就整整错过一个周期。
    if (tts_busy() || tts_queue_count() > 0)
        return;

    last_summary_ms = millis();

    char text[TTS_TEXT_MAX];
    build_summary(text, sizeof(text));

    tts_say(text, TTS_LEVEL_INFO);
    Serial.printf("[link] 周期汇总：%s\n", text);
}

#endif // TTS_SUMMARY_ENABLE

// ============================================================
//  入口
// ============================================================
void linkage_init()
{
    // 静默期的起点。必须在 setup() 里 proto_init() 【之后】调用。
    //
    // 不能拿 millis() 直接和 ONLINE_MUTE_MS 比：proto_init() 里要先连
    // WiFi、再连 MQTT、最后订阅主题，这一段在现场可能要十几秒。从芯片
    // 上电算起的话，连接慢的时候静默期会在订阅建立之前就整段用完，
    // 等于没设——而订阅一建立，服务器立刻就会把当前在线的板全推过来，
    // 正是要屏蔽的那一批。
    mute_start_ms = millis();

    Serial.printf("[link] 静默期起算：%lu 秒内不播报设备上线\n",
                  ONLINE_MUTE_MS / 1000);
}

void linkage_on_message(const char *type, const char *src, JsonObjectConst body)
{
#if TTS_LOCAL_LINKAGE
    if (strcmp(type, "sys") == 0)
        on_sys(src, body);
    else if (strcmp(type, "evt") == 0)
        on_evt(body);
    else if (strcmp(type, "dat") == 0)
        on_dat(body);
    else if (strcmp(type, "ack") == 0)
        on_ack(body);
#else
    (void)type;
    (void)src;
    (void)body;
#endif
}

void linkage_loop()
{
#if TTS_LOCAL_LINKAGE && TTS_SUMMARY_ENABLE
    summary_tick();
#endif
}

// ============================================================
//  覆盖协议层 mqtt_proto.cpp 里的弱函数
//
//  本板订阅了 report 与 online，其他板的报文从这里进来。
//  注意：本板自己发的报文在协议层就已经按 src 过滤掉了，
//  所以这里收到的都是别人发的，不会自己念自己的回执。
// ============================================================
void proto_on_other(const char *type, const char *src, JsonObjectConst body)
{
    linkage_on_message(type, src, body);
}
