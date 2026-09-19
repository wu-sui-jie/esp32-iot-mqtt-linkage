// ==========================================================
//  1 号板 · 舵机模块（servo）
//  文件：servo.cpp
//
//  角度校验、非阻塞平滑转动状态机、板载 LED 转动指示。
//
//  【为什么不用 delay 一次转到位】主循环里跑着 MQTT 收发，一次卡住近 2 秒
//  会收不到命令、回执延迟，卡久了还会被服务器判超时踢下线。
//  所以转动拆成"每 10 毫秒走 1 度"，servo_loop() 每次只走一小步。
//
//  协议依据：表 8（舵机数据点）、表 19（执行完成后立即回执）。
// ==========================================================

#include "servo.h"
#include "my_config.h"

#include <ESP32Servo.h>

// ============================================================
//  1 号板：舵机模块
//
//  这里有两个关键设计，都是踩过才写出来的（详见 readme 第四节）：
//
//  1) 平滑转动做成【非阻塞状态机】。
//     最容易想到的写法是：
//         for (a = cur; a != tgt; a += step) { srv.write(a); delay(10); }
//     但那样一次要卡住主循环近 2 秒。主循环里跑着 proto_loop()，
//     它负责 MQTT 心跳与收发，被卡住的后果是：新命令收不到、
//     回执全部延迟、卡久了还会被服务器判定超时踢下线。
//     所以这里每次 servo_loop() 只走 SERVO_STEP_DEG 度，立刻返回。
//
//  2) 转动没结束前 servo_busy() 返回 true。
//     协议层据此把 ack 挂起，等舵机真的停稳了再回执（协议表 19）。
//     这件事由 mqtt_proto.cpp 里的 device_busy() 弱函数钩子接起来，
//     本文件末尾的强定义覆盖了协议层里的默认实现。
// ============================================================

static Servo srv;

static int cur_deg = SERVO_HOME_DEG; // 当前角度（按轨迹推算）
static int tgt_deg = SERVO_HOME_DEG; // 目标角度
static uint32_t last_step_ms = 0;    // 上一小步的时刻

// 板载 LED1：低电平点亮（课堂例程的接法），转动期间亮，到位即灭
static inline void led_moving(bool on)
{
    digitalWrite(PIN_LED1, on ? LOW : HIGH);
}

// 接上舵机、回到中位、配好转动指示 LED
void servo_init()
{
    pinMode(PIN_LED1, OUTPUT);
    led_moving(false);

    // 舵机标准 50 Hz，必须在 attach 之前设
    srv.setPeriodHertz(50);
    // 500~2500 us 脉宽对应 0~180°
    srv.attach(SERVO_PIN, 500, 2500);

    cur_deg = tgt_deg = SERVO_HOME_DEG;
    srv.write(cur_deg);
    Serial.printf("[servo] 初始化完成，已回到中位 %d°\n", cur_deg);
}

// 受理一个目标角度（范围已由 main.cpp 校验过）
bool servo_set_angle(int angle)
{
    if (angle < SERVO_ANGLE_MIN || angle > SERVO_ANGLE_MAX)
        return false; // 兜底：main.cpp 已校验过，这里再挡一次

    // 目标角度没变就不重启轨迹：既不重复写 PWM，
    // 也不会把"已经在往这儿转"的过程打断重来
    if (angle == tgt_deg)
    {
        Serial.printf("[servo] 目标角度仍是 %d°，不重复动作\n", angle);
        return true;
    }

    tgt_deg = angle;
    Serial.printf("[servo] 收到目标角度 %d°，当前 %d°，开始平滑转动\n", tgt_deg, cur_deg);
    return true;
}

// 是否还在转动。协议层据此决定这条命令的 ack 什么时候发
bool servo_busy()
{
    return cur_deg != tgt_deg;
}

// 当前角度（按步进轨迹推算）
int servo_angle()
{
    return cur_deg;
}

// 每次只走一小步就返回，绝不阻塞 MQTT 收发
void servo_loop()
{
    if (cur_deg == tgt_deg)
        return; // 已到位，什么都不做

    uint32_t now = millis();
    if (now - last_step_ms < SERVO_STEP_MS)
        return; // 还没到下一小步的时刻
    last_step_ms = now;

    led_moving(true); // 转动期间点亮

    // 走一小步，最后一步收到目标角度上，不会冲过头
    if (tgt_deg > cur_deg)
        cur_deg = min(cur_deg + SERVO_STEP_DEG, tgt_deg);
    else
        cur_deg = max(cur_deg - SERVO_STEP_DEG, tgt_deg);

    srv.write(cur_deg);

    if (cur_deg == tgt_deg)
    {
        led_moving(false);
        Serial.printf("[servo] 到位 %d°\n", cur_deg);
    }
}

// ============================================================
//  覆盖协议层 mqtt_proto.cpp 里的弱函数
//
//  返回 true 时，协议层会把这条命令的 ack 挂起，
//  等舵机停稳之后再发出去（协议表 19：执行完成后立即回执）。
// ============================================================
bool device_busy()
{
    return servo_busy();
}
