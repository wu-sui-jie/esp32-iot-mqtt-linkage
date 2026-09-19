#include "light_sensor.h"
#include "mqtt_proto.h"

#if HAS_LIGHT_SENSOR

#include <Wire.h>

// ============================================================
//  8 号板：光照度传感器模块
//
//  两种传感器共用一个模块：
//    BH1750 是数字传感器，I2C 读两个字节，除以 1.2 就是 lx，
//    数值可直接使用；光敏电阻是模拟量，只能按 ADC 读数粗略
//    换算到 0~LIGHT_ADC_MAX_LUX，精度低但不需要额外模块。
//  优先用 BH1750，读不到才退回光敏电阻。
//
//  【本版的一处改动】原实现里 BH1750 读到一半失败会掉到光敏电阻
//  的换算分支。但用 BH1750 时通常根本没接光敏电阻，那一路的读数
//  是悬空引脚的噪声，上报出去就是一条假数据。现在改成：BH1750
//  读取失败就跳过本次上报，并打印提示——宁可少一条数据，
//  也不要发一条错的（与 4 号板 sht30 的做法一致）。
// ============================================================

static bool use_bh1750 = false;      // 是否在用 BH1750
static float last_lux = -1.0f;       // 最近一次有效读数
static bool was_connected = false;   // 上一轮的 MQTT 连接状态
static unsigned long last_report_ms = 0;

// ============================================================
//  BH1750 驱动（不需要第三方库）
//
//  上电流程按数据手册：先发上电命令 0x01，再发连续测量命令 0x10
//  （连续 H 分辨率模式，1 lx 精度，测量时间约 120 ms）。
//  之后每次读 2 个字节即可，不必重复发命令。
// ============================================================
static bool bh1750_begin()
{
    Wire.begin(LIGHT_SDA, LIGHT_SCL);

    // 空传输一次：有应答说明地址上有器件
    Wire.beginTransmission(BH1750_ADDR);
    if (Wire.endTransmission() != 0)
        return false;

    Wire.beginTransmission(BH1750_ADDR);
    Wire.write(0x01); // 上电
    Wire.endTransmission();

    Wire.beginTransmission(BH1750_ADDR);
    Wire.write(0x10); // 连续 H 分辨率模式
    Wire.endTransmission();

    return true;
}

// 读一次 BH1750。成功返回 true 并写进 *out
static bool bh1750_read(float *out)
{
    if (Wire.requestFrom(BH1750_ADDR, 2) != 2)
        return false;
    if (Wire.available() < 2)
        return false;

    uint16_t raw = (uint16_t)((Wire.read() << 8) | Wire.read());
    *out = raw / 1.2f; // 数据手册的换算公式
    return true;
}

// 读一次光敏电阻，按 ADC 读数粗略换算到 0~LIGHT_ADC_MAX_LUX
static float adc_read()
{
    int raw = analogRead(LIGHT_ADC_PIN);
    return (float)map(raw, 0, LIGHT_ADC_MAX, 0, LIGHT_ADC_MAX_LUX);
}

// ============================================================
//  统一入口：读一次光照度。失败返回 false，不改动 *out
// ============================================================
static bool read_lux(float *out)
{
    if (use_bh1750)
        return bh1750_read(out);

    *out = adc_read();
    return true;
}

// 上报光照度（dat，1 位小数，协议表 15 与 §8.1）
static void report_lux(float lux)
{
    // 先按 1 位小数格式化再用 serialized() 原样写出，
    // 是为了保证 120.0 这种整数值也输出成 120.0 而不是 120 ——
    // 否则不同时刻的小数位数会不一致，平台画曲线时会跳。
    // 缓冲必须是 static：serialized() 只保存引用，
    // 真正序列化发生在 proto_send_dat() 里。
    static char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", lux);

    JsonArray samples = proto_dat_samples();
    JsonObject o = samples.createNestedObject();
    o["key"] = "illuminance";
    o["value"] = serialized(buf);

    proto_send_dat(DEV_LIGHT_SENSOR);
}

// ============================================================
//  对外接口
// ============================================================
void light_sensor_init()
{
    use_bh1750 = bh1750_begin();

    if (use_bh1750)
        Serial.println("[light] 检测到 BH1750 数字光照度传感器（I2C，地址 0x23）");
    else
        Serial.printf("[light] 未检测到 BH1750，改用光敏电阻（ADC GPIO%d）\n",
                      LIGHT_ADC_PIN);

    last_report_ms = millis();
}

void light_sensor_loop()
{
    unsigned long now = millis();

    // ---- MQTT 刚连上（含断线重连）：立刻补报一次 ----
    // 平台重连后不用等满一个周期就能看到读数。
    bool connected = proto_connected();
    if (connected && !was_connected)
    {
        float lux;
        if (read_lux(&lux))
        {
            last_lux = lux;
            report_lux(lux);
            Serial.printf("[light] MQTT 已连接，补报光照度 %.1f lx\n", lux);
        }
        last_report_ms = now;
    }
    was_connected = connected;

    // ---- 周期上报 ----
    if (now - last_report_ms < LIGHT_REPORT_PERIOD_MS)
        return;
    last_report_ms = now;

    float lux;
    if (!read_lux(&lux))
    {
        Serial.println("[light] 读取失败（I2C 无应答），本周期不上报");
        return;
    }

    last_lux = lux;
    report_lux(lux);

    Serial.printf("[light] 光照度 %.1f lx\n", lux);
}

float light_sensor_lux()
{
    return last_lux;
}

bool light_sensor_is_bh1750()
{
    return use_bh1750;
}

#endif
