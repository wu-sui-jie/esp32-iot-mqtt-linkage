// ==========================================================
//  协议公共层 · 实现（八块板共用同一份）
//  文件：mqtt_proto.cpp
//
//  本文件是全工程唯一依赖 MQTT 库的地方，换库只改这里：
//    · WiFi 与 MQTT 的连接、断线重连、遗嘱设置
//    · 收到报文后的信封校验与分流（cmd 入队，其余交给 proto_on_other）
//    · 发送序号维护，ack / dat / evt / sys 的组包与发布
//    · device_busy() 与 proto_on_other() 两个弱符号钩子的默认实现
//
//  【回调里不能 publish】arduino-mqtt 规定在消息回调里调用
//  publish / subscribe 会死锁，所以回调只做解析与入队，
//  真正的发送都放在 proto_loop() 里做。
//
//  【八份副本必须逐字节一致】改一份要同步其余七份。
// ==========================================================

#include "mqtt_proto.h"

#include <WiFi.h>
#include <MQTTClient.h>

// ============================================================
//  本文件是唯一依赖具体 MQTT 库的地方。
//  换库只改这里，其他文件不受影响。
//
//  ── 各板之间的差异全部由 my_config.h 的宏控制，本文件本身各板一致 ──
//    ACK_AFTER_DONE    1 = 执行器做完动作才回执（1 号板舵机用）
//    SUB_EXTRA_REPORT  1 = 额外订阅 report（联动旁听用）
//    SUB_EXTRA_ONLINE  1 = 额外订阅 online（联动旁听用）
//  宏没定义时按 0 处理，所以不定义它们的工程行为不受影响。
// ============================================================

static WiFiClient net;

// 收发缓冲区都显式放大。
// 默认只有 128 字节，装不下带 param 的 ack 与 dat 报文。
// 两个参数分别是「读缓冲」「写缓冲」，都设大以免长报文被截断。
static MQTTClient client(MQTT_BUF_SIZE, MQTT_BUF_SIZE);

// ---------------- 发送序号 ----------------
// 由本板独立计数，从 1 开始，到 SEQ_MAX 后回到 1（协议表 2）
static uint32_t tx_seq = 1;

// 取下一个发送序号。从 1 开始自增，到达上限后回到 1（协议表 2）
static int next_seq()
{
    int s = (int)tx_seq;
    tx_seq++;
    if (tx_seq > SEQ_MAX)
        tx_seq = 1;
    return s;
}

// ---------------- 待处理的命令 ----------------
// 【重要】arduino-mqtt 要求回调里不要调用 publish()/subscribe()，否则可能死锁。
// 所以回调只做「解析 + 校验 + 存进这里」，真正的回执在 proto_loop() 里发。
static bool pending = false;
static char pend_device[16];
static char pend_action[24];
static int pend_seq = 0;
// 384 而不是 256：协议限制单条报文 400 字节以内，其中 param 可能占去
// 大部分（例如 tts 的长播报文本），文档太小会让 param 静默丢失。
static StaticJsonDocument<384> pend_param;

#if ACK_AFTER_DONE
// ---------------- 已受理、等执行器做完再回执的命令 ----------------
// 舵机转 180° 要 1.8 秒，收到命令时不能马上回 ok，得等它停稳。
// 这条命令从 pending 挪到这里存着，到位置了再发 ack。
static bool wait_ack = false;
static char wait_device[16];
static char wait_action[24];
static int wait_seq = 0;
static StaticJsonDocument<384> wait_param;
#endif

// ---------------- 发送用的缓冲区 ----------------
static StaticJsonDocument<384> tx_doc;  // ack / sys 共用
static StaticJsonDocument<384> dat_doc; // dat 专用

static bool publish_to(const char *topic, const char *payload, bool retain, int qos);

// ============================================================
//  弱符号的默认实现
//
//  不实现这两个函数（或实现成这个默认样子）的板，行为与原来完全一致。
//  1 号板在 servo.cpp 覆盖 device_busy，7 号板在 linkage.cpp 覆盖
//  proto_on_other，其他板什么都不用做。
// ============================================================
__attribute__((weak)) bool device_busy()
{
    return false;
}

// 弱符号默认实现：不订阅 report / online 的板用不到这个钩子
__attribute__((weak)) void proto_on_other(const char *type, const char *src,
                                          JsonObjectConst body)
{
    (void)type;
    (void)src;
    (void)body;
}

// ============================================================
//  收到报文：解析信封 → 校验 → 分流
//    cmd → 存进待处理缓冲，主循环里执行
//    其他（evt / dat / sys）→ 交给 proto_on_other（7 号板联动用）
// ============================================================
static void on_mqtt_message(String &topic, String &payload)
{
    (void)topic;

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        Serial.printf("[proto] JSON 解析失败: %s\n", err.c_str());
        return;
    }

    const char *ver = doc["ver"] | "";
    const char *type = doc["type"] | "";
    const char *src = doc["src"] | "";

    // 协议 §3.2：先判断版本，不一致就忽略并在串口提示
    if (strcmp(ver, PROTO_VER) != 0)
    {
        Serial.printf("[proto] 协议版本不符（收到 \"%s\"），忽略\n", ver);
        return;
    }

    // 【防自回环】本板自己发的报文一律丢掉。
    // 7 号板为了联动订阅了 report 与 online，会收到自己发的 ack 与 online；
    // 这一句是硬保证：只要 src 是本板板号就不处理，回环不可能形成
    // （协议 §8.4 担心的正是这件事，见 readme 第四节 P4）。
    if (atoi(src) == BOARD_ID)
        return;

    JsonObjectConst body = doc["body"].as<JsonObjectConst>();

    // 非 cmd 报文：本层不处理，交给可选钩子
    if (strcmp(type, "cmd") != 0)
    {
        proto_on_other(type, src, body);
        return;
    }

    // dst 为目标板号，0 表示广播
    int dst = doc["dst"] | -1;
    if (dst != 0 && dst != BOARD_ID)
        return;

    const char *device = body["device"] | "";
    const char *action = body["action"] | "";

    if (device[0] == '\0' || action[0] == '\0')
    {
        Serial.println("[proto] cmd 缺少 body.device 或 body.action，忽略");
        return;
    }

    // 存进待处理缓冲（拷贝一份，回调返回后 doc 就失效了）
    strncpy(pend_device, device, sizeof(pend_device) - 1);
    pend_device[sizeof(pend_device) - 1] = '\0';
    strncpy(pend_action, action, sizeof(pend_action) - 1);
    pend_action[sizeof(pend_action) - 1] = '\0';
    pend_seq = doc["seq"] | 0;

    pend_param.clear();
    JsonObjectConst p = body["param"].as<JsonObjectConst>();
    if (!p.isNull())
        pend_param.set(p);

    pending = true;

    Serial.printf("[proto] <- cmd seq=%d device=%s action=%s\n",
                  pend_seq, pend_device, pend_action);
}

// ============================================================
//  发布的小工具：序列化 + 打印 + 发送
// ============================================================
static bool publish_doc(JsonDocument &doc, const char *topic, bool retain, int qos)
{
    char buf[384];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf))
    {
        Serial.println("[proto] 报文序列化失败或超出缓冲，放弃发送");
        return false;
    }

    Serial.printf("[proto] -> %s : %s\n", topic, buf);
    return publish_to(topic, buf, retain, qos);
}

// 发布到主题的最后一层封装，失败时打印库返回的错误码
static bool publish_to(const char *topic, const char *payload, bool retain, int qos)
{
    bool ok = client.publish(topic, payload, retain, qos);
    if (!ok)
        Serial.printf("[proto] 发布失败，错误码 = %d\n", (int)client.lastError());
    return ok;
}

// ============================================================
//  回执（type = ack，QoS 1）
//
//  seq 由调用方传入：正常回执用命令的 seq，挂起后再发的用存的 wait_seq，
//  两者都是「所回应的那条命令的 seq」，平台据此配对。
// ============================================================
static void send_ack(const char *device, const char *action, const char *result,
                     int seq, JsonObject ackParam)
{
    tx_doc.clear();
    tx_doc["ver"] = PROTO_VER;
    tx_doc["type"] = "ack";
    tx_doc["seq"] = seq;
    tx_doc["src"] = BOARD_ID_STR;
    tx_doc["dst"] = 0;

    JsonObject b = tx_doc.createNestedObject("body");
    b["device"] = device;
    b["action"] = action;
    b["result"] = result;
    if (!ackParam.isNull() && ackParam.size() > 0)
        b["param"] = ackParam; // 回发执行后的实际状态

    publish_doc(tx_doc, TOPIC_REPORT, false, 1);
}

// ============================================================
//  上线通知（type = sys，QoS 1，Retain）
// ============================================================
static void publish_online()
{
    tx_doc.clear();
    tx_doc["ver"] = PROTO_VER;
    tx_doc["type"] = "sys";
    tx_doc["seq"] = next_seq();
    tx_doc["src"] = BOARD_ID_STR;
    tx_doc["dst"] = 0;

    JsonObject b = tx_doc.createNestedObject("body");
    b["device"] = "sys";
    b["event"] = "online";
    b["uptime"] = (uint32_t)(millis() / 1000);

    publish_doc(tx_doc, TOPIC_ONLINE, true, 1);
}

// ============================================================
//  处理待回执的命令
// ============================================================
static void flush_pending()
{
#if ACK_AFTER_DONE
    // 有一条命令已经受理、执行器还在动作：等它做完再把 ack 发出去。
    // 这一轮不再受理新命令（return），免得两条命令的回执交错。
    if (wait_ack)
    {
        if (device_busy())
            return; // 还没到位，下一轮再来看

        send_ack(wait_device, wait_action, "ok", wait_seq,
                 wait_param.as<JsonObject>());
        wait_ack = false;
        return;
    }
#endif

    if (!pending)
        return;
    pending = false;

    StaticJsonDocument<384> ackParamDoc;
    JsonObject ackParam = ackParamDoc.to<JsonObject>();

    ProtoResult r = handle_device_cmd(pend_device, pend_action,
                                      pend_param.as<JsonObjectConst>(), ackParam);

    if (r == PROTO_IGNORE)
    {
        // 协议要求「设备被要求执行动作时」必须回执；不认识的设备/动作不是
        // 发给本板模块的，不必回执
        Serial.printf("[proto] 未知 device/action（%s / %s），不回执\n",
                      pend_device, pend_action);
        return;
    }

    if (r == PROTO_FAIL)
    {
        // 参数非法：立即回 fail，不涉及执行器动作，没有挂起的必要
        send_ack(pend_device, pend_action, "fail", pend_seq, ackParam);
        return;
    }

#if ACK_AFTER_DONE
    // 命令已受理，但执行器还在动作：把回执挂起，到位置了再发
    if (device_busy())
    {
        strncpy(wait_device, pend_device, sizeof(wait_device) - 1);
        wait_device[sizeof(wait_device) - 1] = '\0';
        strncpy(wait_action, pend_action, sizeof(wait_action) - 1);
        wait_action[sizeof(wait_action) - 1] = '\0';
        wait_seq = pend_seq;
        wait_param.clear();
        wait_param.to<JsonObject>().set(ackParam);
        wait_ack = true;
        Serial.printf("[proto] 执行器动作中，seq=%d 的 ack 挂起，到位后再发\n", wait_seq);
        return;
    }
#endif

    send_ack(pend_device, pend_action, "ok", pend_seq, ackParam);
}

// ============================================================
//  连接
// ============================================================
static void proto_connect()
{
    // 客户端编号全局唯一：esp32- + 板号 + MAC（协议 §8.4 建议的格式）
    String cid = "esp32-" + String(BOARD_ID) + "-" + WiFi.macAddress();

    // 遗嘱消息：本板掉线时由服务器代发 sys/offline
    // 必须在 connect 之前设置（协议 §6，且库要求如此）
    char will[192];
    snprintf(will, sizeof(will),
             "{\"ver\":\"%s\",\"type\":\"sys\",\"seq\":2,\"src\":\"%d\",\"dst\":0,"
             "\"body\":{\"device\":\"sys\",\"event\":\"offline\"}}",
             PROTO_VER, BOARD_ID);
    client.setWill(TOPIC_ONLINE, will, true, 1); // Retain + QoS 1

    while (!client.connected())
    {
        Serial.printf("[proto] MQTT 连接中... 错误码 = %d\n", (int)client.lastError());
        if (client.connect(cid.c_str()))
        {
            Serial.println("[proto] MQTT 已连接");
        }
        else
        {
            delay(2000);
        }
    }

    // 只订阅 cmd，绝不订阅 report —— 否则会收到自己发的报文，造成自回环
    client.subscribe(TOPIC_CMD, 1);
    Serial.printf("[proto] 已订阅 %s\n", TOPIC_CMD);

#if SUB_EXTRA_REPORT
    // 7 号板专用：联动播报需要旁听其他板的上报。
    // 协议 §8.4 一般要求各板不订阅 report，本板的理由与防护见 readme 第四节 P4，
    // 自回环已由 on_mqtt_message 开头的 src 过滤彻底堵死。
    client.subscribe(TOPIC_REPORT, 1);
    Serial.printf("[proto] 已订阅 %s（联动旁听）\n", TOPIC_REPORT);
#endif

#if SUB_EXTRA_ONLINE
    client.subscribe(TOPIC_ONLINE, 1);
    Serial.printf("[proto] 已订阅 %s（联动旁听）\n", TOPIC_ONLINE);
#endif

    publish_online();
}

// 连 WiFi、连 MQTT、订阅主题、发布上线通知。setup() 里第一个调用
void proto_init()
{
    Serial.begin(115200);
    delay(200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[proto] 连接 WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.printf("[proto] WiFi 已连接，IP = %s\n", WiFi.localIP().toString().c_str());

    client.begin(MQTT_SERVER, MQTT_PORT, net);
    client.onMessage(on_mqtt_message);

    proto_connect();
}

// 当前是否已连上 MQTT
bool proto_connected()
{
    return client.connected();
}

// 主循环里每次都要调用：断线重连、收报文、发出待回执
void proto_loop()
{
    if (!client.connected())
        proto_connect();

    client.loop();
    delay(10); // 库建议：loop 后加个小延时，改善 WiFi 稳定性

    flush_pending();
}

// ============================================================
//  周期数据上报（type = dat）
// ============================================================
JsonArray proto_dat_samples()
{
    dat_doc.clear();
    dat_doc["ver"] = PROTO_VER;
    dat_doc["type"] = "dat";
    dat_doc["seq"] = 0; // 发送时填真实序号
    dat_doc["src"] = BOARD_ID_STR;
    dat_doc["dst"] = 0;

    JsonObject b = dat_doc.createNestedObject("body");
    b["device"] = ""; // 发送时填

    return b.createNestedArray("samples");
}

// 把 proto_dat_samples() 填好的数据发布出去（QoS 0，不保留）
bool proto_send_dat(const char *device)
{
    dat_doc["seq"] = next_seq();
    dat_doc["body"]["device"] = device;

    // 协议 §8.2：report 的周期数据用 QoS 0，不保留
    return publish_doc(dat_doc, TOPIC_REPORT, false, 0);
}

// ============================================================
//  事件上报（type = evt，QoS 1）
//
//  与 dat 分开用一份文档：evt 是状态跳变这一类必须送达的报文，
//  不能被随后的周期数据覆盖掉。两份文档各自独立，互不干扰。
// ============================================================
static StaticJsonDocument<256> evt_doc;

// 上报一条事件（QoS 1，不保留）：设备名、事件名、等级 0~3
bool proto_send_evt(const char *device, const char *event, int level)
{
    evt_doc.clear();
    evt_doc["ver"] = PROTO_VER;
    evt_doc["type"] = "evt";
    evt_doc["seq"] = next_seq();
    evt_doc["src"] = BOARD_ID_STR;
    evt_doc["dst"] = 0;

    JsonObject b = evt_doc.createNestedObject("body");
    b["device"] = device;
    b["event"] = event;
    b["level"] = level;

    // 协议表 18：report 的事件与回执用 QoS 1，不保留
    return publish_doc(evt_doc, TOPIC_REPORT, false, 1);
}
