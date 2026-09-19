#include "mqtt_proto.h"

#include <WiFi.h>
#include <MQTTClient.h>

// ============================================================
//  本文件是唯一依赖具体 MQTT 库的地方。
//  换库只改这里，其他文件不受影响。
// ============================================================

static WiFiClient net;

// 收发缓冲区都显式放大。
// 默认只有 128 字节，装不下带 param 的 ack 与 dat 报文。
// 两个参数分别是「读缓冲」「写缓冲」，都设大以免长报文被截断。
static MQTTClient client(MQTT_BUF_SIZE, MQTT_BUF_SIZE);

// ---------------- 发送序号 ----------------
// 由本板独立计数，从 1 开始，到 SEQ_MAX 后回到 1（协议表 2）
static uint32_t tx_seq = 1;

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
static StaticJsonDocument<256> pend_param;

// ---------------- 发送用的缓冲区 ----------------
static StaticJsonDocument<384> tx_doc;  // ack / sys 共用
static StaticJsonDocument<384> dat_doc; // dat 专用

static bool publish_to(const char *topic, const char *payload, bool retain, int qos);

// ============================================================
//  收到报文：解析信封 → 校验 → 存进待处理缓冲
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
    int dst = doc["dst"] | -1;
    int seq = doc["seq"] | 0;

    // 协议 §3.2：先判断版本，不一致就忽略并在串口提示
    if (strcmp(ver, PROTO_VER) != 0)
    {
        Serial.printf("[proto] 协议版本不符（收到 \"%s\"），忽略\n", ver);
        return;
    }

    // 只处理下行命令
    if (strcmp(type, "cmd") != 0)
        return;

    // dst 为目标板号，0 表示广播
    if (dst != 0 && dst != ID)
        return;

    JsonObjectConst body = doc["body"].as<JsonObjectConst>();
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
    pend_seq = seq;

    pend_param.clear();
    JsonObjectConst p = body["param"].as<JsonObjectConst>();
    if (!p.isNull())
        pend_param.set(p);

    pending = true;

    Serial.printf("[proto] <- cmd seq=%d device=%s action=%s\n", seq, device, action);
}

// ============================================================
//  发布一个小工具：序列化 + 打印 + 发送
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

static bool publish_to(const char *topic, const char *payload, bool retain, int qos)
{
    bool ok = client.publish(topic, payload, retain, qos);
    if (!ok)
        Serial.printf("[proto] 发布失败，错误码 = %d\n", (int)client.lastError());
    return ok;
}

// ============================================================
//  回执（type = ack，QoS 1）
// ============================================================
static void send_ack(const char *device, const char *action, const char *result,
                     JsonObject ackParam)
{
    tx_doc.clear();
    tx_doc["ver"] = PROTO_VER;
    tx_doc["type"] = "ack";
    tx_doc["seq"] = pend_seq; // 复用命令的 seq，平台据此配对
    tx_doc["src"] = String(ID);
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
    tx_doc["src"] = String(ID);
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
    if (!pending)
        return;
    pending = false;

    StaticJsonDocument<256> ackParamDoc;
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

    send_ack(pend_device, pend_action, (r == PROTO_OK) ? "ok" : "fail", ackParam);
}

// ============================================================
//  连接
// ============================================================
static void proto_connect()
{
    // 客户端编号全局唯一：esp32- + MAC（协议 §8.4）
    String cid = "esp32-" + WiFi.macAddress();

    // 遗嘱消息：本板掉线时由服务器代发 sys/offline
    // 必须在 connect 之前设置（协议 §6，且库要求如此）
    char will[192];
    snprintf(will, sizeof(will),
             "{\"ver\":\"%s\",\"type\":\"sys\",\"seq\":2,\"src\":\"%d\",\"dst\":0,"
             "\"body\":{\"device\":\"sys\",\"event\":\"offline\"}}",
             PROTO_VER, ID);
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

    publish_online();
}

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

    client.begin(mqttServer, mqttPort, net);
    client.onMessage(on_mqtt_message);

    proto_connect();
}

bool proto_connected()
{
    return client.connected();
}

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
    dat_doc["src"] = String(ID);
    dat_doc["dst"] = 0;

    JsonObject b = dat_doc.createNestedObject("body");
    b["device"] = ""; // 发送时填

    return b.createNestedArray("samples");
}

bool proto_send_dat(const char *device)
{
    dat_doc["seq"] = next_seq();
    dat_doc["body"]["device"] = device;

    // 协议 §8.2：report 的周期数据用 QoS 0，不保留
    return publish_doc(dat_doc, TOPIC_REPORT, false, 0);
}
