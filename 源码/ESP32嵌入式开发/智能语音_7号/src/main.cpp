#include <Arduino.h>
#include "my_config.h"
#include "mqtt_proto.h"
#include "tts.h"
#include "linkage.h"

// ============================================================
//  7 号板：智能语音播报模块（执行器）
//
//  本模块有两条播报来源，都在这里汇合：
//    路径 A  平台下发 —— 本文件的 handle_device_cmd()
//    路径 B  本地联动 —— linkage.cpp，报文经 proto_on_other() 进来
//  两条最终都调 tts_say() 排队，由 tts_loop() 统一下发。
//
//  linkage.cpp 内部又分三层：事件即时播报、阈值告警、周期数据汇总。
// ============================================================

ProtoResult handle_device_cmd(const char *device, const char *action,
                              JsonObjectConst param, JsonObject ackParam)
{
    if (strcmp(device, DEV_TTS) != 0)
        return PROTO_IGNORE; // 不是本板实现的设备（lcd 等本板不做）

    if (strcmp(action, "play_text") != 0)
        return PROTO_IGNORE; // 不认识的动作

    JsonVariantConst v = param["text"];
    if (!v.is<const char *>())
    {
        Serial.println("[tts] play_text 缺少 text 或 text 不是字符串");
        return PROTO_FAIL;
    }

    const char *text = v.as<const char *>();
    if (text[0] == '\0')
    {
        Serial.println("[tts] play_text 的 text 为空，不播报");
        return PROTO_FAIL;
    }

    // 平台命令用最高等级：人点的那一下必须能盖过本地联动播报。
    // 若前一条还在播，本条进队列；ack 仍立即回，表示"已受理"。
    // 只有连队列都挤不进去（整队都是平台命令）才回 fail。
    if (!tts_say(text, TTS_LEVEL_CMD))
    {
        Serial.println("[tts] 播报队列已满，本条命令被丢弃");
        return PROTO_FAIL;
    }

    // 协议表 14 的 ack 语义就是「文本已下发至语音模块，开始播报」，
    // 载荷里不带 param，所以这里不往 ackParam 写任何字段。
    (void)ackParam;
    return PROTO_OK;
}

void setup()
{
    proto_init(); // 连接 WiFi 与 MQTT（内部已 Serial.begin）
    tts_init();   // 打开语音模块串口，播报开机提示
}

void loop()
{
    proto_loop();   // MQTT 收发 + 回执
    tts_loop();     // 播报队列在这里真正下发，回调里只入队
    linkage_loop(); // 第三层：周期数据汇总（空闲时播）
}
