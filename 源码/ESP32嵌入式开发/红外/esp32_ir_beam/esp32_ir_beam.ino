/*
 * 红外对射传感器模块 —— ESP32 端程序（3 号板）
 * 功能：
 *   检测红外对射模块是否被遮挡，状态变化时立即上报 evt 事件；30 秒心跳；
 *   累计遮挡次数 count 随遮挡事件上报（选做，协议表 20）
 * 协议：《MQTT通信协议规范_第1组》V2.0
 *   订阅 202609_PP2/1/cmd    (QoS 1，只订阅 cmd，不订阅 report)
 *   发布 202609_PP2/1/report (evt 事件与 dat 数据，QoS 0)
 *   发布 202609_PP2/1/online (sys 上线报文与遗嘱离线报文，Retain)
 * 消息格式（信封 + 载荷）：
 *   {"ver":"1.0","type":"evt","seq":1,"src":"3","dst":0,
 *    "body":{"device":"ir_beam","event":"blocked","level":2}}
 * 接线：
 *   红外对射 DO -> GPIO14
 *   VCC -> 3.3V 或 5V
 *   GND -> GND
 * 依赖库：PubSubClient、ArduinoJson
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ================= 可修改配置 =================
const char* WIFI_SSID = "wusuijie";              // WiFi 名称
const char* WIFI_PASS = "11111111";         // WiFi 密码
const char* MQTT_HOST = "yunyismart.tech";  // MQTT 服务器
const int   MQTT_PORT = 1883;

const int   ID  = 3;                        // 本板板号（协议表 7：3 号板 = 红外对射传感器）
const char* SRC = "3";                      // 上行报文 src（板号字符串）

// 主题定义（协议 二、Topic 设计）
const char* TOPIC_CMD    = "202609_PP2/1/cmd";     // 下行：各平台 → 各板（本板订阅）
const char* TOPIC_REPORT = "202609_PP2/1/report";  // 上行：各板 → 各平台（本板发布）
const char* TOPIC_ONLINE = "202609_PP2/1/online";  // 上行：在线状态（本板发布）

const int IR_PIN = 14;                      // 红外对射模块数字输入，HIGH=遮挡

const unsigned long IR_DEBOUNCE = 50;       // 去抖(ms)，协议 5.3：连续稳定 50ms 才确认
const unsigned long IR_MIN_GAP   = 500;     // 限流(ms)，协议 5.3：同一状态 500ms 内不重复上报
const unsigned long IR_HEARTBEAT = 30000;   // 心跳(ms)，协议 5.3：状态不变时每 30 秒重发
// =============================================

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// 红外对射状态
bool blocked          = false;              // 当前原始状态：true=遮挡
bool confirmedBlocked = false;              // 去抖确认后的状态
unsigned long rawChangeAt   = 0;            // 原始状态最近变化时刻（去抖计时）
unsigned long lastBlockedAt = 0;            // 最近一次上报 blocked 的时刻（限流）
unsigned long lastClearAt   = 0;            // 最近一次上报 clear 的时刻（限流）
unsigned long lastBeatAt    = 0;            // 最近一次心跳时刻
unsigned long blockedCount  = 0;            // 累计遮挡次数（选做，协议表 20）
unsigned long seq = 1;                      // 报文序号，从 1 开始自增（协议 3.2）

// ---------- MQTT 报文（信封 + 载荷） ----------
unsigned long nextSeq() {
    unsigned long s = seq++;
    if (seq == 0) seq = 1;                  // 达到上限后回到 1（协议 3.2）
    return s;
}

// 构造信封并发布；PubSubClient 仅支持 QoS 0 发布（协议表 1 允许 QoS 0 或 1）
void publishMsg(const char* topic, const char* type, const String &body, bool retain = false) {
    String payload = "{\"ver\":\"1.0\",\"type\":\"";
    payload += type;
    payload += "\",\"seq\":";
    payload += nextSeq();
    payload += ",\"src\":\"";
    payload += SRC;
    payload += "\",\"dst\":0,\"body\":";
    payload += body;
    payload += "}";
    client.publish(topic, payload.c_str(), retain);
    Serial.print("[发布] "); Serial.println(payload);
}

// 遮挡状态事件（evt，协议表 10：blocked 等级 2，clear 等级 0）
void publishEvent(const char* event, int level) {
    String body = "{\"device\":\"ir_beam\",\"event\":\"";
    body += event;
    body += "\",\"level\":";
    body += level;
    body += "}";
    publishMsg(TOPIC_REPORT, "evt", body);
}

// 累计遮挡次数（dat，选做，协议表 20）
void publishCount() {
    String body = "{\"device\":\"ir_beam\",\"samples\":[{\"key\":\"count\",\"value\":";
    body += blockedCount;
    body += "}]}";
    publishMsg(TOPIC_REPORT, "dat", body);
}

// 上线报文（sys，Retain，协议 六、表 17）
void publishOnline() {
    String body = "{\"device\":\"sys\",\"event\":\"online\",\"uptime\":";
    body += millis() / 1000;
    body += "}";
    publishMsg(TOPIC_ONLINE, "sys", body, true);
}

// ---------- 下行命令回调（协议 8.1 解析流程） ----------
void onMessage(char* topic, byte* payload, unsigned int len) {
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, (const char*)payload, len)) {
        Serial.println("[忽略] JSON 解析失败");
        return;
    }
    // 先判断协议版本，不一致忽略（协议 3.2）
    const char* ver = doc["ver"];
    if (!ver || strcmp(ver, "1.0") != 0) {
        Serial.printf("[忽略] 协议版本不匹配: %s\n", ver ? ver : "(空)");
        return;
    }
    // 只处理下行命令，dst 为 0（广播）或本板板号（协议 8.1）
    const char* type = doc["type"];
    if (!type || strcmp(type, "cmd") != 0) return;
    int dst = doc["dst"];
    if (dst != 0 && dst != ID) return;
    // 本板为纯传感器板，无下行数据点；通用模块未焊接，忽略（协议 5.9）
    const char* device = doc["body"]["device"];
    Serial.printf("[收到命令] device=%s，本板无对应执行器，忽略\n", device ? device : "(空)");
}

// 连接（重连）MQTT
void reconnectMQTT() {
    while (!client.connected()) {
        String mac = WiFi.macAddress(); mac.replace(":", "");
        // 客户端编号：esp32-板号-MAC，全局唯一（协议 8.4）
        String clientId = "esp32-" + String(ID) + "-" + mac.substring(6);
        // 遗嘱消息：异常断电/掉线时由服务器代发 offline（协议 六、表 17）
        String willMsg = "{\"ver\":\"1.0\",\"type\":\"sys\",\"seq\":";
        willMsg += nextSeq();
        willMsg += ",\"src\":\"";
        willMsg += SRC;
        willMsg += "\",\"dst\":0,\"body\":{\"device\":\"sys\",\"event\":\"offline\"}}";
        Serial.printf("连接 MQTT %s ...\n", clientId.c_str());
        if (client.connect(clientId.c_str(), TOPIC_ONLINE, 1, true, willMsg.c_str())) {
            Serial.println("MQTT 连接成功");
            publishOnline();                        // 上线报文（Retain）
            client.subscribe(TOPIC_CMD, 1);         // 只订阅 cmd 主题（协议 8.4）
            // 上线先报一次当前状态
            publishEvent(confirmedBlocked ? "blocked" : "clear", confirmedBlocked ? 2 : 0);
            lastBeatAt = millis();                  // 心跳从连接成功算起
        } else {
            Serial.printf("连接失败 rc=%d，2 秒后重试\n", client.state());
            delay(2000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    // 红外对射模块
    pinMode(IR_PIN, INPUT);
    blocked = confirmedBlocked = (digitalRead(IR_PIN) == HIGH);  // HIGH=遮挡
    Serial.println(confirmedBlocked ? "红外初始状态: 遮挡" : "红外初始状态: 通畅");

    // 连接 WiFi
    Serial.printf("连接 WiFi %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi 已连接，IP = ");
    Serial.println(WiFi.localIP());

    // 配置 MQTT
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setKeepAlive(60);
    client.setBufferSize(512);                    // 协议 8.1：接收缓冲区建议 512
    client.setCallback(onMessage);
}

void loop() {
    if (!client.connected()) reconnectMQTT();
    client.loop();

    // 红外对射状态变化检测（去抖 50ms + 限流 500ms，协议 5.3）
    bool rawBlocked = (digitalRead(IR_PIN) == HIGH);
    if (rawBlocked != blocked) {
        blocked = rawBlocked;
        rawChangeAt = millis();
    }
    if (blocked != confirmedBlocked && millis() - rawChangeAt >= IR_DEBOUNCE) {
        // 同一状态 500ms 内不重复上报（协议 5.3）
        unsigned long &lastAt = blocked ? lastBlockedAt : lastClearAt;
        if (millis() - lastAt >= IR_MIN_GAP) {
            confirmedBlocked = blocked;
            lastAt = millis();
            lastBeatAt = millis();                // 状态确认后重置心跳周期
            Serial.println(confirmedBlocked ? "红外: 遮挡" : "红外: 通畅");
            publishEvent(confirmedBlocked ? "blocked" : "clear", confirmedBlocked ? 2 : 0);
            if (confirmedBlocked) {
                blockedCount++;                   // 每次确认遮挡后加 1（协议表 20）
                publishCount();
            }
        }
    }

    // 心跳：状态持续不变时每 30 秒重发当前状态（协议 5.3）
    if (millis() - lastBeatAt >= IR_HEARTBEAT) {
        lastBeatAt = millis();
        publishEvent(confirmedBlocked ? "blocked" : "clear", confirmedBlocked ? 2 : 0);
    }
}
