#include <Arduino.h>
#include "UTF8ToGB2312.h"

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

void TTS_init()
{
    Serial2.begin(115200, SERIAL_8E1, 16, 17); // 使用偶校验模式
    sendTTSMessage("欢迎使用TTS智能语音播报系统");
}

void setup()
{
    Serial.begin(115200);

    TTS_init();
}

String tts_message = "";
int cnt = 0;

void loop()
{
    String str = (String)(cnt++);
    sendTTSMessage(str);
    delay(1000);

    if (Serial.available())
    {
        tts_message = Serial.readString();
        sendTTSMessage(tts_message);
    }
}
