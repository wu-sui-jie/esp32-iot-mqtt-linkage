#include "linkage.h"
#include "my_config.h"
#include "tts.h"

// ============================================================
//  7 号板：本地联动播报
//
//  这是本模块区别于别的执行器的地方：它不只等平台下命令，还会
//  【听】其他板子在干什么，然后把运行情况播报出来。
//
//  触发源与播报内容（规则表，改动请同步 readme）：
//    6 号 flame         evt detected      "警告，检测到火焰"        紧急
//    6 号 flame         evt clear         "火焰警报已解除"          恢复
//    3 号 ir_beam       evt blocked       "注意，检测到遮挡"        警告
//    3 号 ir_beam       evt clear         "遮挡已恢复"              恢复
//    4 号 sht30         temperature 高    "温度过高，当前 XX.X 度"  警告
//    4 号 sht30         humidity 高       "湿度偏高，当前 XX%"     提示
//    8 号 light_sensor  illuminance 低    "光线过暗，请注意"        提示
//    任意板             sys online        "X 号设备已上线"          提示
//    任意板             sys offline       "X 号设备已离线"          警告
//    1 号 servo         ack ok, angle=X   "舵机已转到 X 度"         提示
//
//  舵机那条特殊：协议表 8 只给舵机定义了 cmd 与 ack，它没有 evt，
//  "舵机转了"这件事只能从 ack 的 param.angle 里读出来（见 on_ack）。
//
//  【本文件只在 MQTT 回调链上被调用】所以这里只做「判断 + 入队」，
//  真正的串口下发在 tts_loop() 里做。arduino-mqtt 规定回调里不能
//  publish / subscribe（会死锁），慢操作也应当挪出回调。
// ============================================================

// 每条规则各自的冷却计时，防止火焰闪烁 / 遮挡抖动把播报刷屏
enum Rule
{
    R_FLAME_ON = 0, // 检测到火焰
    R_FLAME_OFF,    // 火焰解除
    R_IR_ON,        // 光束遮挡
    R_IR_OFF,       // 遮挡恢复
    R_TEMP,         // 温度过高
    R_HUMI,         // 湿度过高
    R_LIGHT,        // 光线过暗
    R_ONLINE,       // 设备上线
    R_OFFLINE,      // 设备离线
    R_SERVO,        // 舵机动作完成
    R_COUNT
};

static uint32_t last_fire_ms[R_COUNT] = {0};

// 等级决定冷却时间：紧急 / 警告用短的，提示 / 恢复用长的
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
// 舵机播报要短冷却：演示时经常连着改几次角度，用默认的 60 秒会显得像坏了。
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
//  sys：设备上下线（发布在 online 主题，Retain）
// ============================================================
static void on_sys(const char *src, JsonObjectConst body)
{
    const char *ev = body["event"] | "";
    int board = atoi(src);

    // 开机静默期：online 用了 Retain，本板一订阅就会把当前在线的板
    // 全部推过来，不屏蔽的话开机就是一串"X 号设备已上线"。
    if (millis() < ONLINE_MUTE_MS)
    {
        Serial.printf("[link] 开机静默期内，忽略 %d 号板的 %s\n", board, ev);
        return;
    }

    char text[48];
    if (strcmp(ev, "online") == 0)
    {
        snprintf(text, sizeof(text), "%d 号设备已上线", board);
        say_rule(R_ONLINE, text, TTS_LEVEL_INFO);
    }
    else if (strcmp(ev, "offline") == 0)
    {
        snprintf(text, sizeof(text), "%d 号设备已离线", board);
        say_rule(R_OFFLINE, text, TTS_LEVEL_WARN);
    }
}

// ============================================================
//  evt：火焰与对射遮挡，状态一变就要立刻响
//
//  火焰这一条【按轮次播报，不按报文条数】：6 号板在火焰持续期间每 5 秒
//  重发一次 detected（防平台漏报），那些重发不是新事件，同一轮里只按
//  FLAME_REMIND_MS 提醒，不逐条播。收到 clear 本轮结束并重新武装，
//  于是"灭火后重新点火"立刻就能播。
//  上一版是拿 30 秒冷却去压这些重发的，代价是演示时点火常常不响
//  （点火时刻离上次播报不足 30 秒就被压掉），所以改成现在这样。
// ============================================================
static bool     flame_alarming    = false; // 本轮火焰是否已播过告警
static uint32_t last_flame_rx_ms  = 0;     // 上一条 detected 的到达时刻
static uint32_t last_flame_say_ms = 0;     // 上一次播"检测到火焰"的时刻

static void on_evt(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    const char *ev = body["event"] | "";

    if (strcmp(device, "flame") == 0)
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
            // 被冷却或队列挡下的那次不记账，后面的重发还有机会补播
            // （与 on_ack 里舵机角度的做法一致）
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
            // 这里同样走火焰自己的短冷却，不用"恢复"档的 60 秒，
            // 免得演示时每轮只有第一次灭火听得见。
            if (flame_alarming)
            {
                flame_alarming = false;
                say_rule_cd(R_FLAME_OFF, "火焰警报已解除",
                            TTS_LEVEL_RECOVER, COOLDOWN_FLAME_MS);
            }
        }
    }
    else if (strcmp(device, "ir_beam") == 0)
    {
        if (strcmp(ev, "blocked") == 0)
            say_rule(R_IR_ON, "注意，检测到遮挡", TTS_LEVEL_WARN);
        else if (strcmp(ev, "clear") == 0)
            say_rule(R_IR_OFF, "遮挡已恢复", TTS_LEVEL_RECOVER);
    }
}

// ============================================================
//  dat：温湿度与光照，把实测值拼进句子一起念
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
            if (strcmp(key, "temperature") == 0 && val >= TEMP_HIGH)
            {
                char text[48];
                snprintf(text, sizeof(text), "温度过高，当前 %.1f 度", val);
                say_rule(R_TEMP, text, TTS_LEVEL_WARN);
            }
            else if (strcmp(key, "humidity") == 0 && val >= HUMI_HIGH)
            {
                char text[48];
                snprintf(text, sizeof(text), "湿度偏高，当前 %d%%", (int)(val + 0.5f));
                say_rule(R_HUMI, text, TTS_LEVEL_INFO);
            }
        }
        else if (strcmp(device, "light_sensor") == 0)
        {
            if (strcmp(key, "illuminance") == 0 && val < LIGHT_LOW)
                say_rule(R_LIGHT, "光线过暗，请注意", TTS_LEVEL_INFO);
        }
    }
}

// ============================================================
//  ack：舵机动作播报
//
//  协议表 8 只给 1 号板定义了 cmd 与 ack 两类报文，它没有 evt、也没有
//  dat，所以"舵机转了多少度"只能从它的 ack 里读出来。
//  1 号板的 ack 是【转到位之后】才发出来的，所以这里播报的时机正好
//  是舵机刚停稳，"舵机已转到 180 度"说出来和动作是合上的。
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
//  入口
// ============================================================
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
