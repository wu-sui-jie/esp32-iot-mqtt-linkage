#pragma once
#include <Arduino.h>
#include "my_config.h"

#if HAS_TTS

// TTS初始化
void tts_init();

// TTS播放声音
void tts_play(String msg);

// TTS的循环
void tts_loop();

#endif