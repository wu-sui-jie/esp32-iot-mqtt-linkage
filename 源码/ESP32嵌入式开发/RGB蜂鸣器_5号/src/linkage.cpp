#include "linkage.h"
#include "my_config.h"
#include "mqtt_proto.h"
#include "rgb.h"
#include "buzzer.h"

// ============================================================
//  5 号板：本地联动
//
//  规则表（原创于协议表 20 的扩展用法，改动请同步 readme）：
//    触发源                  条件                 动作
//    ───────────────────────────────────────────────────────────
//    8 号 light_sensor   illuminance ≤ 50      RGB 暖白常亮（照明）
//    8 号 light_sensor   illuminance ≥ 120     RGB 熄灭
//    6 号 flame          evt detected          RGB 多彩循环 + 蜂鸣器间歇报警
//    6 号 flame          evt clear             回到"按光照决定"的状态
//
//  两个场景抢同一组灯和同一个蜂鸣器，所以必须仲裁。
//  优先级：火焰（3，最高） > 平台手动命令（2） > 光照照明（1）。
//
//  【仲裁用"状态重算"，不是"来一个事件改一次灯"】
//  如果写成"收到火焰事件就把灯设成红色"，那么事件以不同顺序到达时
//  最终状态会不一样：先收到火焰 clear 再收到光照过暗，和反过来，
//  结果完全不同，还会出现"火焰灭了灯却还红着"这类残留。
//  所以这里把三个输入都记成状态（flame_alarm / light_dark /
//  manual_override），任何一个变了就整体重算一次场景，
//  无论事件以什么顺序到达，最终状态都一致。
//
//  【回调里只记状态，动作在主循环做】
//  mqtt_proto.cpp 的回调 → 本文件的 linkage_on_message 里只更新
//  上面那几个布尔量并置 need_update 标志；真正的 rgb_apply / 蜂鸣器
//  动作、以及 evt 上报都在 linkage_loop() 里做。
//  原因是 arduino-mqtt 规定回调里不能 publish（会死锁），
//  联动要发 scene_fire / scene_light 这类 evt，必须挪出回调。
// ============================================================

// 联动的输入设备名（协议表 7 里 6 号板与 8 号板的 device）
#define LINK_DEV_FLAME "flame"
#define LINK_DEV_LIGHT_SENSOR "light_sensor"

// 三个输入状态
static bool flame_alarm = false;     // 6 号板正在报火焰
static bool light_dark = false;      // 8 号板光照低于开灯阈值
static bool light_known = false;     // 是否收到过光照数据（没数据时不开灯）
static bool manual_override = false; // 平台手动接管中

static bool need_update = false; // 有输入变化，待重算场景

// 场景定义，数字越大优先级越高
enum Scene
{
    SCENE_OFF = 0,  // 全灭
    SCENE_LIGHT = 1, // 照明
    SCENE_FIRE = 3   // 火焰报警
};

static Scene cur_scene = SCENE_OFF;

// ============================================================
//  手动接管
//
//  平台下发 set_color / buzzer 命令时调用，之后联动不再抢灯，
//  直到下一次传感器状态变化把 manual_override 清掉。
//  这样"人点的那一下"永远盖得过联动，不会刚设好颜色就被改回去。
// ============================================================
void linkage_manual_takeover()
{
    manual_override = true;
    need_update = false;

    // 手动接管时停掉火焰动画：否则 rgb_loop() 每 300ms 会把
    // 平台设的颜色覆盖掉，看起来像命令没生效。
    rgb_set_fire(false);
    buzzer_set_alarm(false);

    Serial.println("[link] 平台手动接管，联动暂停到下次传感器状态变化");
}

// ============================================================
//  应用场景
// ============================================================
static void apply_scene(Scene s)
{
    if (s == cur_scene)
        return;

    cur_scene = s;

    switch (s)
    {
    case SCENE_FIRE:
        rgb_set_fire(true);      // 多彩循环（rgb_loop 逐帧推进）
        buzzer_set_alarm(true);  // 间歇报警
        proto_send_evt(DEV_RGB, "scene_fire", 3);
        Serial.println("[link] 场景 → 火焰报警（多彩循环 + 蜂鸣器）");
        break;

    case SCENE_LIGHT:
        rgb_set_fire(false);
        buzzer_set_alarm(false);
        rgb_apply(LIGHT_SCENE_R, LIGHT_SCENE_G, LIGHT_SCENE_B);
        proto_send_evt(DEV_RGB, "scene_light", 1);
        Serial.println("[link] 场景 → 光照照明");
        break;

    default:
        rgb_set_fire(false);
        buzzer_set_alarm(false);
        rgb_apply(0, 0, 0);
        proto_send_evt(DEV_RGB, "scene_off", 0);
        Serial.println("[link] 场景 → 熄灭");
        break;
    }
}

// ============================================================
//  按优先级重算场景
// ============================================================
static void recompute()
{
    need_update = false;

    if (flame_alarm)
    {
        apply_scene(SCENE_FIRE);
        return;
    }

    // 光照照明要满足三个条件：收到过数据、确实过暗、平台没接管
    if (light_dark && light_known && !manual_override)
    {
        apply_scene(SCENE_LIGHT);
        return;
    }

    apply_scene(SCENE_OFF);
}

// ============================================================
//  evt：火焰（6 号板）
// ============================================================
static void on_evt(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    const char *ev = body["event"] | "";

    if (strcmp(device, LINK_DEV_FLAME) != 0)
        return;

    if (strcmp(ev, "detected") == 0)
    {
        // 6 号板在火焰持续期间每 5 秒重发一次 detected，
        // 这里重复置位是无害的（apply_scene 只在场景真变了才动作）
        flame_alarm = true;
        manual_override = false; // 安全事件优先级高于手动接管
        need_update = true;
        Serial.println("[link] 收到火焰告警");
    }
    else if (strcmp(ev, "clear") == 0)
    {
        flame_alarm = false;
        manual_override = false;
        need_update = true;
        Serial.println("[link] 火焰告警解除");
    }
}

// ============================================================
//  dat：光照（8 号板）
//
//  迟滞：低于 LIGHT_DARK_LUX 才开灯，高于 LIGHT_BRIGHT_LUX 才关灯。
//  两个阈值分开是必要的——只用一个门槛的话，光照在门槛附近抖动时
//  灯会跟着闪个不停。
// ============================================================
static void on_dat(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    if (strcmp(device, LINK_DEV_LIGHT_SENSOR) != 0)
        return;

    JsonArrayConst samples = body["samples"].as<JsonArrayConst>();
    for (JsonVariantConst s : samples)
    {
        const char *key = s["key"] | "";
        if (strcmp(key, "illuminance") != 0)
            continue;

        JsonVariantConst v = s["value"];
        if (!v.is<int>() && !v.is<float>())
            continue;

        float lux = v.as<float>();
        bool dark_now = light_dark;

        if (!light_known)
        {
            // 第一次收到光照数据：直接按阈值判断
            dark_now = (lux < LIGHT_DARK_LUX);
            light_known = true;
        }
        else if (!light_dark && lux < LIGHT_DARK_LUX)
        {
            dark_now = true; // 变暗，开灯
        }
        else if (light_dark && lux > LIGHT_BRIGHT_LUX)
        {
            dark_now = false; // 变亮，关灯
        }

        if (dark_now != light_dark)
        {
            light_dark = dark_now;
            manual_override = false; // 光照状态真变了，恢复联动
            need_update = true;

            Serial.printf("[link] 光照 %.1f lx → %s\n",
                          lux, light_dark ? "过暗，开照明" : "转亮，关照明");
        }
        return;
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
    else if (strcmp(type, "dat") == 0)
        on_dat(body);
}

void linkage_loop()
{
    if (!need_update)
        return;
    recompute();
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
