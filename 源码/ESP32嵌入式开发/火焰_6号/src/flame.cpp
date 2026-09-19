// ==========================================================
//  6 号板 · 火焰传感器（flame）
//  文件：flame.cpp
//
//  不用绝对阈值，而用"相对基线的偏移"。
//
//  不同环境下无火时的静止读数差别很大（光照、电源、模块个体差异），
//  写死阈值换个场合就失效。开机先采样 2 秒取平均得到基线，
//  之后偏离超过阈值就判为有火。
//
//  三处工程化处理（依据协议 5.6 与表 19）：
//    · 去抖 50ms      —— 火焰跳动会让读数抖
//    · 限流 500ms     —— 同一触发事件不重复上报
//    · 持续重发 5 秒  —— 火还在烧就定期再说一次，防平台漏报
//
//  被限流挡下的状态跳变会延迟补发，绝不丢弃——状态跳变只有一次机会，
//  丢了就再也不会发，下游会一直停在报警状态。
// ==========================================================

#include "flame.h"
#include "mqtt_proto.h"

#if HAS_FLAME

// ============================================================
//  6 号板：火焰传感器模块
//
//  检测原理：火焰会发出红外线，接收管把红外强度变成电压，
//  经 AO 引脚进入 12 位 ADC（读数 0~4095）。
//
//  这里【不用绝对阈值，而用"相对基线的偏移"】：不同环境下
//  无火时的静止读数差别很大（光照、电源、模块个体差异都会影响），
//  写死一个绝对阈值换个场合就失效。开机先采样 2 秒取平均得到基线，
//  之后只要读数偏离基线超过 FLAME_THRESHOLD 就判为有火焰。
//  副作用是校准期间不能有火焰，否则基线会被抬高。
//
//  三处工程化处理（依据协议 5.6 与表 19）：
//    · 去抖 50 ms —— 火焰跳动会让读数抖，稳定后才认
//    · 限流 500 ms —— 同一触发事件不重复上报，避免刷屏
//    · 持续重发 5 秒 —— 火焰还在烧就定期再说一次，防平台漏报
// ============================================================

// ---------------- 状态 ----------------
static int base = 0; // AO 的无火基线

static bool raw_last = false;             // 上一次的原始判定（去抖用）
static bool flame_state = false;          // 去抖确认后的状态
static bool was_connected = false;        // 上一轮的 MQTT 连接状态

static unsigned long raw_since_ms = 0;      // 原始判定最近一次变化的时刻
static unsigned long last_evt_ms = 0;       // 最近一次上报事件（限流）

// 被限流挡下的状态跳变，记在这里等窗口过去再补发。
// 【绝不能直接丢掉】状态跳变一块板只发生一次，丢了就没有第二次机会。
// 现场最典型的翻车方式：点火后很快移开打火机，clear 正好落在 500ms 的
// 限流窗口里被丢弃，而 flame_state 这时已经改成"无火"，于是再也不会重发，
// 下游（5 号板的声光报警）就一直停在火焰报警上停不下来。
static bool pending_evt = false;
static bool pending_state = false;
static unsigned long last_detected_ms = 0;  // 最近一次上报 detected（重发计时）
static unsigned long last_dat_ms = 0;       // 最近一次上报强度

static int last_intensity = 0; // 最近一次的强度原始值

// ============================================================
//  读 AO，返回原始读数；偏离基线多少写进 *out_diff
//  引脚在 my_config.h 里（原理图确认是 IO33）
// ============================================================
static int read_ao(int *out_diff)
{
    int a = analogRead(PIN_FLAME_AO);
    if (out_diff)
        *out_diff = abs(a - base);
    return a;
}

// ============================================================
//  开机基线校准：连续采样 2 秒取平均
//
//  这一步会阻塞约 2 秒。放在 setup 阶段是可以接受的：
//  现场演示时板上电后本来就要等它稳定，而且这 2 秒远小于
//  MQTT 的 60 秒 keepalive，不会掉线。
// ============================================================
static void calibrate()
{
    Serial.printf("[flame] AO 基线校准中（请勿点火），%lu 毫秒...\n",
                  (unsigned long)FLAME_CALIBRATE_MS);

    long sum = 0;
    int n = 0;
    unsigned long t0 = millis();
    while (millis() - t0 < FLAME_CALIBRATE_MS)
    {
        sum += analogRead(PIN_FLAME_AO);
        n++;
        delay(20);
    }

    base = (int)(sum / n);

    Serial.printf("[flame] 基线 IO%d=%d  判定阈值=%d\n",
                  PIN_FLAME_AO, base, FLAME_THRESHOLD);
}

// ============================================================
//  上报一次状态跳变（detected / clear）
// ============================================================
static void send_state_evt(bool has_flame, int ao, int diff)
{
    if (has_flame)
    {
        Serial.printf("[flame] >>> 检测到火焰 <<<（IO=%d 偏离=%d 阈值=%d）\n",
                      ao, diff, FLAME_THRESHOLD);
        proto_send_evt(DEV_FLAME, "detected", 3); // 等级 3 紧急
        last_detected_ms = millis();
    }
    else
    {
        Serial.printf("[flame] >>> 火焰消失 <<<（IO=%d 偏离=%d）\n", ao, diff);
        proto_send_evt(DEV_FLAME, "clear", 0); // 等级 0 恢复
    }
}

// 补发被限流挡下的那次状态跳变。窗口一到就发，所以最多迟 500ms，
// 但绝不会像以前那样凭空消失。
static void flush_pending_evt()
{
    if (!pending_evt)
        return;

    unsigned long now = millis();
    if (now - last_evt_ms < FLAME_LIMIT_MS)
        return; // 限流窗口还没过去

    pending_evt = false;
    last_evt_ms = now;

    int diff = 0;
    int ao = read_ao(&diff);
    Serial.println("[flame] 补发刚才被限流挡下的状态事件");
    send_state_evt(pending_state, ao, diff);
}

// ============================================================
//  检测与上报
// ============================================================
static void scan()
{
    unsigned long now = millis();

    int diff = 0;
    int ao = read_ao(&diff);
    bool raw = (diff > FLAME_THRESHOLD); // 偏离超过阈值 = 有火焰

    // ---- 去抖：原始判定变化后重新计时 ----
    if (raw != raw_last)
    {
        raw_last = raw;
        raw_since_ms = now;
        return;
    }
    if (now - raw_since_ms < FLAME_DEBOUNCE_MS)
        return;

    if (raw != flame_state)
    {
        flame_state = raw;

        // 限流：同一触发事件 500ms 内不重复上报
        if (now - last_evt_ms >= FLAME_LIMIT_MS)
        {
            last_evt_ms = now;
            send_state_evt(flame_state, ao, diff);
        }
        else
        {
            // 落在限流窗口里：不丢，记下来等窗口过去补发。
            // 期间状态若再变，会覆盖这里的记录，所以补发的永远是最新状态。
            pending_evt = true;
            pending_state = flame_state;
        }
    }
    else if (flame_state)
    {
        // ---- 持续检测期间每 5 秒重发一次 detected，防平台漏报 ----
        if (now - last_detected_ms >= FLAME_RESEND_MS)
        {
            last_detected_ms = now;
            Serial.println("[flame] 火焰持续中，重发 detected");
            proto_send_evt(DEV_FLAME, "detected", 3);
        }
    }
}

// 上报火焰强度（dat）
static void report_intensity()
{
    int diff = 0;
    int ao = read_ao(&diff);
    last_intensity = ao;

    JsonArray samples = proto_dat_samples();
    JsonObject o = samples.createNestedObject();
    o["key"] = "intensity";
    o["value"] = ao;

    proto_send_dat(DEV_FLAME);

    Serial.printf("[flame] 强度=%d（偏离基线 %d）  状态=%s\n",
                  ao, diff, flame_state ? "有火" : "无火");
}

// ============================================================
//  对外接口
// ============================================================
void flame_init()
{
    analogReadResolution(FLAME_ADC_BITS);

    calibrate();

    // 校准结束后用当前读数初始化状态，避免上电瞬间误报
    int diff = 0;
    read_ao(&diff);
    raw_last = flame_state = (diff > FLAME_THRESHOLD);

    last_dat_ms = millis();
    last_detected_ms = millis();

    Serial.printf("[flame] 初始化完成，当前状态：%s\n",
                  flame_state ? "有火" : "无火");
}

// 重连补报、状态检测、强度周期上报，以及被限流挡下的事件补发
void flame_loop()
{
    unsigned long now = millis();

    // ---- MQTT 刚连上（含断线重连）：补报一次当前状态 ----
    bool connected = proto_connected();
    if (connected && !was_connected)
    {
        if (flame_state)
        {
            proto_send_evt(DEV_FLAME, "detected", 3);
            last_detected_ms = now;
        }
        else
        {
            proto_send_evt(DEV_FLAME, "clear", 0);
        }
        Serial.println("[flame] MQTT 已连接，补报当前状态");
    }
    was_connected = connected;

    scan();
    flush_pending_evt(); // 补发被限流挡下的状态事件

    // ---- 强度周期上报（协议表 19：每 1 秒）----
    if (now - last_dat_ms >= FLAME_DAT_PERIOD_MS)
    {
        last_dat_ms = now;
        report_intensity();
    }
}

// 去抖确认后的当前状态
bool flame_detected()
{
    return flame_state;
}

// 最近一次采样的强度原始值（0~4095）
int flame_intensity()
{
    return last_intensity;
}

#endif
