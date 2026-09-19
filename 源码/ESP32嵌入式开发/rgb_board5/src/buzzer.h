#pragma once

#include <Arduino.h>
#include "my_config.h"

#if HAS_BUZZER

// ============================================================
//  5 号板：蜂鸣器
//
//  协议表 12：下行 set_power{"on":true/false}，持续鸣叫 / 停止
//  协议表 20：下行 beep{"duration":50~5000}，按时长鸣叫（选做）
//
//  两种鸣叫方式并存，由本模块统一管理状态：
//    · 持续（set_power）  —— 一直响到收到关闭命令
//    · 定时（beep）       —— 响够时长自动停
//    · 间歇报警（火焰联动）—— 响 300ms 停 300ms 循环
//  三者抢同一个硬件，所以必须由一个状态机统管，
//  不能让三个来源各自去写 GPIO。
// ============================================================

// 上电初始化：配好输出，从静音开始
void buzzer_init();

// 主循环里每次都要调用：处理定时鸣叫的停止、间歇报警的节拍
void buzzer_loop();

// 持续鸣叫 / 停止（协议表 12）
void buzzer_set_power(bool on);

// 定时鸣叫（协议表 20，时长 50~5000 毫秒），到点自动停
void buzzer_beep(unsigned int duration_ms);

// 开 / 关间歇报警（火焰联动用）：响 BEEP_ON 停 BEEP_OFF 循环
void buzzer_set_alarm(bool on);

// 当前是否在发声
bool buzzer_is_on();

#endif
