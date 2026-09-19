#include "linkage.h"
#include "my_config.h"
#include "mqtt_proto.h"
#include "fan.h"

// ============================================================
//  2 号板：本地联动
//
//  规则表（改动请同步 readme）：
//    触发源            条件                          动作
//    ────────────────────────────────────────────────────────────
//    4 号 sht30   temperature ≥ LINK_TEMP_ON     开风扇
//    4 号 sht30   humidity    ≥ LINK_HUMI_ON     开风扇
//    两者都回落到 OFF 阈值以下并持续
//    LINK_FAN_HOLD_MS                            关风扇
//
//  【为什么用两个阈值】温度在 30 ℃ 上下浮动时，单阈值会让继电器
//  一秒内吸合好几次。开阈值 30、关阈值 28，中间 2 ℃ 是缓冲区，
//  越过它才算真的"变热"或"变凉"。
//
//  【为什么关风扇要等 10 秒】传感器偶尔会跳一个数（I2C 受扰、
//  热源晃动），单次跳数不该让风扇启停。条件持续正常 10 秒才关，
//  抖动造成的短暂越界就自然被吞掉了。
//
//  【平台命令的优先级】平台手动开关风扇永远有效，联动不会去覆盖它；
//  联动只在"环境条件发生变化"的那一刻动作。所以演示时可以随时
//  手动控制，不会被联动抢走。
// ============================================================

// 联动监听的设备名（协议表 7 里 4 号板的 device）
#define LINK_DEV_SHT30 "sht30"

// ---------------- 联动状态 ----------------
static bool temp_high = false; // 温度超开阈值
static bool humi_high = false; // 湿度超开阈值

static bool auto_running = false;      // 联动已经开了风扇
static unsigned long clear_since = 0;  // 条件恢复正常的起始时刻（0 = 尚未恢复）

// ============================================================
//  dat：温湿度（4 号板）
//
//  4 号板每 5 秒把温度与湿度放在同一条报文的 samples 里发过来
//  （协议 §5.4），这里逐个数据点看，各自维护自己的状态。
//  回调里只更新状态，不动风扇也不上报——见 linkage_loop()。
// ============================================================
static void on_dat(JsonObjectConst body)
{
    const char *device = body["device"] | "";
    if (strcmp(device, LINK_DEV_SHT30) != 0)
        return;

    JsonArrayConst samples = body["samples"].as<JsonArrayConst>();
    for (JsonVariantConst s : samples)
    {
        const char *key = s["key"] | "";

        JsonVariantConst v = s["value"];
        if (!v.is<int>() && !v.is<float>())
            continue; // value 不是数值就不管

        float val = v.as<float>();

        if (strcmp(key, "temperature") == 0)
        {
            if (!temp_high && val >= LINK_TEMP_ON)
            {
                temp_high = true;
                Serial.printf("[link] 温度 %.1f ℃ 达到开阈值 %.1f\n", val, LINK_TEMP_ON);
            }
            else if (temp_high && val <= LINK_TEMP_OFF)
            {
                temp_high = false;
                Serial.printf("[link] 温度 %.1f ℃ 回落到关阈值 %.1f 以下\n", val, LINK_TEMP_OFF);
            }
        }
        else if (strcmp(key, "humidity") == 0)
        {
            if (!humi_high && val >= LINK_HUMI_ON)
            {
                humi_high = true;
                Serial.printf("[link] 湿度 %.1f %% 达到开阈值 %.1f\n", val, LINK_HUMI_ON);
            }
            else if (humi_high && val <= LINK_HUMI_OFF)
            {
                humi_high = false;
                Serial.printf("[link] 湿度 %.1f %% 回落到关阈值 %.1f 以下\n", val, LINK_HUMI_OFF);
            }
        }
    }
}

// ============================================================
//  入口
// ============================================================
void linkage_on_message(const char *type, const char *src, JsonObjectConst body)
{
    (void)src; // 自己的报文已被协议层按 src 过滤，这里收到的都是别人的

    if (strcmp(type, "dat") == 0)
        on_dat(body);
}

void linkage_loop()
{
    unsigned long now = millis();
    bool too_hot = (temp_high || humi_high);

    if (too_hot)
    {
        clear_since = 0; // 条件不满足的计时作废

        if (!auto_running)
        {
            auto_running = true;
            fan_set_power(true);
            proto_send_evt(DEV_FAN, "auto_on", 1);
            Serial.println("[link] 温湿度过高，联动开启风扇");
        }
        return;
    }

    // ---- 条件已恢复正常，走迟滞 ----
    if (!auto_running)
        return; // 风扇本来就不是联动开的，不干预

    if (clear_since == 0)
    {
        clear_since = now; // 第一次发现恢复正常，开始计时
        return;
    }

    if (now - clear_since < LINK_FAN_HOLD_MS)
        return; // 还没等够，继续观察

    clear_since = 0;
    auto_running = false;
    fan_set_power(false);
    proto_send_evt(DEV_FAN, "auto_off", 0);
    Serial.println("[link] 温湿度恢复正常，联动关闭风扇");
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
