// ==========================================================
//  4 号板 · 温湿度传感器（sht30）
//  文件：sht30.cpp
//
//  I2C 读取 SHT30，温湿度合并成一条 dat 报文上报（协议表 11）。
//
//  三道检查，任一不过就跳过本周期、不上报：
//    ① I2C 有应答   ② 读到的字节数正好 6   ③ 温度与湿度的 CRC 都通过
//
//  厂商例程没做 CRC 校验，不做的后果是读到错位数据也照算不误，
//  会把莫名其妙的温湿度值发出去。
// ==========================================================

#include "sht30.h"
#include "mqtt_proto.h"

#if HAS_SHT30

#include <Wire.h>

// ================ 时序参数 ================
#define REPORT_PERIOD_MS 5000 // 协议 §8.3 表 19：每 5 秒上报一次
#define MEASURE_WAIT_MS 20    // 高重复性测量的最大转换时间约 15 ms，留点余量

// 协议 §5.4 表 11 的测量命令
#define SHT30_CMD_H 0x2C
#define SHT30_CMD_L 0x06

// ================ 状态 ================
static unsigned long last_report_ms = 0;  // 上一次上报（或放弃）的时刻
static unsigned long measure_start_ms = 0; // 本次测量命令发出的时刻
static bool measuring = false;             // 是否处于"已发命令、等待读取"的状态

// ============================================================
//  CRC-8 校验
//  SHT30 数据手册规定：多项式 0x31、初值 0xFF、不反转、不异或输出
//  厂商例程没做这个校验，不做的后果是读到错位数据也照算不误。
// ============================================================
static uint8_t sht30_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

// ============================================================
//  发出测量命令。返回 false 表示 I2C 没有应答
// ============================================================
static bool sht30_start_measure()
{
    Wire.beginTransmission(SHT30_ADDR);
    Wire.write((uint8_t)SHT30_CMD_H);
    Wire.write((uint8_t)SHT30_CMD_L);
    return Wire.endTransmission() == 0;
}

// ============================================================
//  读取 6 字节并换算成温度、湿度
//
//  三道检查，任一不过就返回 false，**不改动输出参数**：
//    ① 实际读到的字节数必须正好是 6
//    ② 温度的 CRC 必须对
//    ③ 湿度的 CRC 必须对
//
//  为什么必须挡住：厂商例程写的是 if (Wire.available() == 6) 才读，
//  一旦 I2C 没回应，data[] 里是未初始化的栈内存，后面的公式照样会
//  算出一个数并上报 —— 就会把莫名其妙的温湿度值发出去。
// ============================================================
static bool sht30_read(float *out_t, float *out_rh)
{
    if (Wire.requestFrom((uint8_t)SHT30_ADDR, (uint8_t)6) != 6)
        return false;

    uint8_t d[6];
    for (uint8_t i = 0; i < 6; i++)
        d[i] = (uint8_t)Wire.read();

    // 字节顺序：T_MSB / T_LSB / T_CRC / RH_MSB / RH_LSB / RH_CRC
    if (sht30_crc8(d, 2) != d[2])
        return false;
    if (sht30_crc8(d + 3, 2) != d[5])
        return false;

    uint16_t raw_t = (uint16_t)(((uint16_t)d[0] << 8) | d[1]);
    uint16_t raw_rh = (uint16_t)(((uint16_t)d[3] << 8) | d[4]);

    float t = (float)raw_t * 175.0f / 65535.0f - 45.0f; // ℃
    float rh = (float)raw_rh * 100.0f / 65535.0f;       // %RH

    // 协议 §3.4 表 5 的取值范围，越界钳位，防止异常值污染数据曲线
    if (t < -40.0f)
        t = -40.0f;
    if (t > 125.0f)
        t = 125.0f;
    if (rh < 0.0f)
        rh = 0.0f;
    if (rh > 100.0f)
        rh = 100.0f;

    *out_t = t;
    *out_rh = rh;
    return true;
}

// ============================================================
//  上报：温度与湿度必须放在同一条 dat 报文的 samples 数组里
//  （协议 §5.4 明确要求，保证两个数据点采集时刻一致）
// ============================================================
static void sht30_report(float t, float rh)
{
    // §8.1 要求"浮点数序列化前先按约定精度取整"。
    // 这里先按 1 位小数格式化成字符串，再用 serialized() 原样写出，
    // 是为了保证 26.0 这种整数值也输出成 26.0 而不是 26 ——
    // 否则同一块板不同时刻的小数位数会不一致，平台画曲线时会跳。
    // serialized() 写出去的是不带引号的 JSON 数值，仍然符合协议
    // "数值不加引号" 的要求。
    //
    // 缓冲必须是 static：serialized() 只保存字符串的引用，
    // 真正序列化发生在 proto_send_dat() 里，临时变量那时已经析构，
    // 用局部变量会写出脏数据。
    static char t_buf[16];
    static char rh_buf[16];
    snprintf(t_buf, sizeof(t_buf), "%.1f", t);
    snprintf(rh_buf, sizeof(rh_buf), "%.1f", rh);

    JsonArray samples = proto_dat_samples();

    JsonObject o1 = samples.createNestedObject();
    o1["key"] = "temperature";
    o1["value"] = serialized(t_buf);

    JsonObject o2 = samples.createNestedObject();
    o2["key"] = "humidity";
    o2["value"] = serialized(rh_buf);

    bool ok = proto_send_dat(DEV_SHT30);
    if (ok)
        Serial.printf("[sht30] 温度 %.1f ℃  湿度 %.1f %%RH\n", t, rh);
}

// ============================================================
//  对外接口
// ============================================================
void sht30_init()
{
    Wire.begin(SHT30_SDA, SHT30_SCL);
    last_report_ms = millis();
}

// 非阻塞采集状态机：到点发测量命令，等转换完成再读取上报
void sht30_loop()
{
    unsigned long now = millis();

    if (!measuring)
    {
        if (now - last_report_ms < REPORT_PERIOD_MS)
            return;

        // 到点了：只发命令，不等结果，立刻返回，避免卡住主循环
        measuring = true;
        measure_start_ms = now;

        if (!sht30_start_measure())
        {
            Serial.println("[sht30] 发测量命令失败（I2C 无应答），本周期跳过");
            measuring = false;
            last_report_ms = now;
        }
        return;
    }

    // 等测量转换完成
    if (now - measure_start_ms < MEASURE_WAIT_MS)
        return;
    measuring = false;

    float t, rh;
    if (!sht30_read(&t, &rh))
    {
        Serial.println("[sht30] 读取失败（字节数不足或 CRC 校验不过），本周期不上报");
        last_report_ms = now;
        return;
    }

    sht30_report(t, rh);

    // 周期从"上报完成"起算，而不是从"开始采集"起算，
    // 避免 I2C 偶尔变慢时上报周期逐渐漂移
    last_report_ms = now;
}

#endif
