#pragma once

#include <Arduino.h>

// ============================================================
//  7 号板：智能语音播报模块
//
//  GB2312 转换、0xFD 帧封装、播报队列与优先级抢占。
//  播报来源有两条：平台下发（main.cpp）与本地联动（linkage.cpp），
//  两条都走本模块的 tts_say() 排队，由 tts_loop() 统一下发。
// ============================================================

// ================ 播报等级 ================
// 数字越大越急，高等级可以抢断正在播报的低等级（readme 第一节）
#define TTS_LEVEL_RECOVER 0   // 恢复：警报解除
#define TTS_LEVEL_INFO 1      // 提示：光线过暗、设备上线等
#define TTS_LEVEL_WARN 2      // 警告：遮挡、温度过高、设备离线
#define TTS_LEVEL_EMERGENCY 3 // 紧急：检测到火焰
// 平台下发的命令等级最高：人点的那一下必须能盖过本地联动播报
#define TTS_LEVEL_CMD 4

// 初始化串口并播报开机提示音
void tts_init();

// 主循环里每次都要调用：从队列里挑一条真正下发给语音模块
void tts_loop();

// 请求播报一段 UTF-8 文本。
// 【只入队，不发送】真正的串口下发在 tts_loop() 里做：
// 本函数会在 MQTT 回调链上被调用，arduino-mqtt 规定回调里不能
// publish / subscribe（会死锁），慢操作也应当挪出回调。
// 返回 true 表示已受理（进队或直接可播），false 表示被丢弃。
bool tts_say(const char *text, uint8_t level);

// 是否正在播报（按估算时长判断，语音模块不回传播放状态）
bool tts_busy();
