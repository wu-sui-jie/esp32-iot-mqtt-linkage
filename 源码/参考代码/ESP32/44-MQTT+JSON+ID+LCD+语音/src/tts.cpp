#include "tts.h"
#include "UTF8ToGB2312.h"

// 用宏来控制是否编译，从而减小ROM占用
#if HAS_TTS

// TTS是否有新任务
static bool tts_newTask = false;
// TTS要播放的文本
static String tts_message = "";

void sendTTSMessage(const String &message)
{
    String utf8_str = message;
    String gb2312_str = GB.get(utf8_str);

    unsigned int len = gb2312_str.length();
    unsigned char buffer[len + 6]; // 包括头字节和长度字节

    // 填充缓冲区
    buffer[0] = 0xFD;
    buffer[1] = (len + 2) >> 8;
    buffer[2] = (len + 2) & 0xFF;
    buffer[3] = 0x01; // cmd byte
    buffer[4] = 0x01; // para byte

    // 复制GB2312编码数据
    for (unsigned int i = 0; i < len; i++)
    {
        buffer[i + 5] = gb2312_str[i];
    }

    // 发送数据
    Serial2.write(buffer, len + 5);
    Serial.println("TTS message sent");
}

// TTS初始化
void tts_init()
{
    Serial2.begin(115200, SERIAL_8E1, 16, 17); // 使用偶校验模式
    Serial.println("TTS module initialized successfully");
    sendTTSMessage("早上好");
}

// TTS播放声音
void tts_play(String msg)
{
    tts_message = msg;
    tts_newTask = true;
}

// TTS的循环
void tts_loop()
{
    if (tts_newTask)
    {
        tts_newTask = false;
        sendTTSMessage(tts_message);
    }
}

#endif