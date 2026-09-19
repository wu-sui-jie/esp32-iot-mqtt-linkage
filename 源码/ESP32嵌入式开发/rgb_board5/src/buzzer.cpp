#include "buzzer.h"

#if HAS_BUZZER

// ============================================================
//  5 号板：蜂鸣器
//
//  【为什么所有的"停"都是记时刻、不是 delay】
//  beep 的时长最长 5 秒，间歇报警更是要一直响下去。如果用
//  delay() 实现，主循环会卡住 5 秒 —— 而主循环里跑着 proto_loop()，
//  它负责 MQTT 心跳与收发报文，卡住的后果是收不到命令、
//  回执延迟，卡久了还会被服务器判超时踢下线。
//  所以这里只记"该停的时刻"，由 buzzer_loop() 每轮看一眼。
//  同一个道理见 1 号板舵机的平滑转动。
//
//  【三种鸣叫方式共用一个状态机】
//  持续鸣叫、定时鸣叫、间歇报警都只写同一个输出，
//  后启动的覆盖先启动的（现场使用时不会同时要两种）。
//  如果让三个来源各自写 GPIO，退出时就会互相打架，
//  出现"报警结束了蜂鸣器还在响"这类问题。
// ============================================================

static bool sounding = false;              // 当前是否在发声

static bool alarm_on = false;              // 是否处于间歇报警模式
static unsigned long alarm_next_ms = 0;    // 间歇报警的下一次翻转时刻
static bool alarm_phase_on = false;        // 间歇报警当前处于"响"还是"停"

static bool timed_beep = false;            // 是否处于定时鸣叫
static unsigned long beep_stop_ms = 0;     // 定时鸣叫的停止时刻

// ============================================================
//  真正驱动硬件
// ============================================================
static void output(bool on)
{
    if (on == sounding)
        return;
    sounding = on;

#if BUZZER_USE_PWM
    // 无源蜂鸣器：LEDC 输出方波，50% 占空比（128/256）音量最大
    ledcWrite(BUZZER_CHANNEL, on ? 128 : 0);
#else
    // 有源蜂鸣器：直接给电平
    digitalWrite(BUZZER_PIN, on ? (BUZZER_ACTIVE_LEVEL ? HIGH : LOW)
                               : (BUZZER_ACTIVE_LEVEL ? LOW : HIGH));
#endif
}

// ============================================================
//  对外接口
// ============================================================
void buzzer_init()
{
#if BUZZER_USE_PWM
    ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
#else
    pinMode(BUZZER_PIN, OUTPUT);
    output(false);
#endif

    Serial.printf("[buzzer] 初始化完成（GPIO%d，%s驱动），当前静音\n",
                  BUZZER_PIN, BUZZER_USE_PWM ? "方波" : "电平");
}

void buzzer_set_power(bool on)
{
    // 持续鸣叫会顶掉定时鸣叫与间歇报警
    timed_beep = false;
    alarm_on = false;

    output(on);

    Serial.printf("[buzzer] %s\n", on ? "持续鸣叫" : "停止");
}

void buzzer_beep(unsigned int duration_ms)
{
    // 协议表 20 规定时长范围 50~5000 毫秒，越界钳位而不是报错：
    // 报错会让整条命令失败，钳位则能尽量把用户想听的响出来。
    if (duration_ms < 50)
        duration_ms = 50;
    if (duration_ms > 5000)
        duration_ms = 5000;

    alarm_on = false;
    timed_beep = true;
    beep_stop_ms = millis() + duration_ms;

    output(true);

    Serial.printf("[buzzer] 鸣叫 %u 毫秒\n", duration_ms);
}

void buzzer_set_alarm(bool on)
{
    if (on == alarm_on)
        return; // 状态没变，不打断正在走的节拍

    alarm_on = on;
    timed_beep = false;

    if (on)
    {
        alarm_phase_on = true;
        alarm_next_ms = millis() + FIRE_BEEP_ON_MS;
        output(true);
        Serial.println("[buzzer] 进入间歇报警");
    }
    else
    {
        output(false);
        Serial.println("[buzzer] 退出间歇报警");
    }
}

void buzzer_loop()
{
    unsigned long now = millis();

    // ---- 定时鸣叫：到点自动停 ----
    if (timed_beep)
    {
        if ((int32_t)(now - beep_stop_ms) >= 0)
        {
            timed_beep = false;
            output(false);
            Serial.println("[buzzer] 定时鸣叫结束");
        }
        return;
    }

    // ---- 间歇报警：响 ON_MS、停 OFF_MS 交替 ----
    if (alarm_on)
    {
        if ((int32_t)(now - alarm_next_ms) >= 0)
        {
            alarm_phase_on = !alarm_phase_on;
            alarm_next_ms = now + (alarm_phase_on ? FIRE_BEEP_ON_MS : FIRE_BEEP_OFF_MS);
            output(alarm_phase_on);
        }
    }
}

bool buzzer_is_on()
{
    return sounding;
}

#endif
