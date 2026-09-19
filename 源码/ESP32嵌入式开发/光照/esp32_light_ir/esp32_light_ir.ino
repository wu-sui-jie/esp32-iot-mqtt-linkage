/*
 * 光照度传感器模块 —— ESP32 端程序（8 号板）
 * 功能：
 *   自动识别光照传感器：BH1750(I2C) 或 光敏电阻(GPIO34)，每 2 秒周期上报 dat
 * 协议：《MQTT通信协议规范_第1组》V2.0
 *   订阅 202609_PP2/1/cmd    (QoS 1，只订阅 cmd，不订阅 report)
 *   发布 202609_PP2/1/report (dat，QoS 0)
 *   发布 202609_PP2/1/online (sys 上线报文与遗嘱离线报文，Retain)
 * 消息格式（信封 + 载荷）：
 *   {"ver":"1.0","type":"dat","seq":1,"src":"8","dst":0,
 *    "body":{"device":"light_sensor","samples":[{"key":"illuminance","value":125.5}]}}
 * 依赖库：PubSubClient、ArduinoJson
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ================= 可修改配置 =================
const char* WIFI_SSID = "wusuijie";              // WiFi 名称
const char* WIFI_PASS = "11111111";         // WiFi 密码
const char* MQTT_HOST = "yunyismart.tech";  // MQTT 服务器
const int   MQTT_PORT = 1883;

const int   ID  = 8;                        // 本板板号（协议表 7：8 号板 = 光照度传感器）
const char* SRC = "8";                      // 上行报文 src（板号字符串）

// 主题定义（协议 二、Topic 设计）
const char* TOPIC_CMD    = "202609_PP2/1/cmd";     // 下行：各平台 → 各板（本板订阅）
const char* TOPIC_REPORT = "202609_PP2/1/report";  // 上行：各板 → 各平台（本板发布）
const char* TOPIC_ONLINE = "202609_PP2/1/online";  // 上行：在线状态（本板发布）

const int LIGHT_PIN  = 34;                  // 光敏电阻模拟输入（用 BH1750 时忽略）

const unsigned long LIGHT_INTERVAL = 2000;  // 光照上报周期(ms)，协议表 19：每 2 秒
// =============================================

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// 光照传感器
bool useBH1750 = false;
const int BH1750_ADDR = 0x23;
unsigned long lastLight = 0;
unsigned long seq = 1;                      // 报文序号，从 1 开始自增（协议 3.2）

// ---------- BH1750 驱动（无需第三方库） ----------
bool bh1750Begin() {
    Wire.begin(21, 22);                       // SDA=GPIO21, SCL=GPIO22
    Wire.beginTransmission(BH1750_ADDR);
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(BH1750_ADDR);
    Wire.write(0x01);                         // 上电命令
    Wire.endTransmission();
    Wire.beginTransmission(BH1750_ADDR);
    Wire.write(0x10);                         // 连续 H 分辨率模式(1 lx)
    Wire.endTransmission();
    return true;
}

float readLux() {
    if (useBH1750) {
        Wire.requestFrom(BH1750_ADDR, 2);
        if (Wire.available() >= 2) {
            uint16_t raw = (Wire.read() << 8) | Wire.read();
            return raw / 1.2f;                // 真实光照度(lx)
        }
    }
    return map(analogRead(LIGHT_PIN), 0, 4095, 0, 1000);  // 光敏电阻估算
}

// ---------- MQTT 报文（信封 + 载荷） ----------
unsigned long nextSeq() {
    unsigned long s = seq++;
    if (seq == 0) seq = 1;                    // 达到上限后回到 1（协议 3.2）
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

// 上报光照度数据（dat，协议表 15：保留 1 位小数）
void publishData(float lux) {
    String body = "{\"device\":\"light_sensor\",\"samples\":[{\"key\":\"illuminance\",\"value\":";
    body += String(lux, 1);
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
            publishData(readLux());                 // 上线先报一次光照数据
        } else {
            Serial.printf("连接失败 rc=%d，2 秒后重试\n", client.state());
            delay(2000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    // 识别光照度传感器
    useBH1750 = bh1750Begin();
    Serial.println(useBH1750 ? "检测到 BH1750 数字光照度传感器(I2C)"
                             : "未检测到 BH1750，使用光敏电阻(ADC GPIO34)");

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

    // 光照度周期上报（协议表 15：每 2 秒，保留 1 位小数）
    if (millis() - lastLight >= LIGHT_INTERVAL) {
        lastLight = millis();
        float lux = round(readLux() * 10) / 10.0f;   // 序列化前按精度取整（协议 8.1）
        Serial.printf("光照度: %.1f lx\n", lux);
        publishData(lux);
    }
}
