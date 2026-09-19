#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* =====================================================================
 *  6 号板 —— 火焰传感器模块（纯 AO 模拟量判断版）
 *  严格遵循《MQTT 通信协议规范 V2.0（第 1 组）》
 *
 *  说明：本版不再依赖 DO（数字量），改为只用 AO（模拟量）判断火焰。
 *       开机先采样 2 秒得到"基线"，之后只要 AO 偏离基线超过阈值
 *       就判为"检测到火焰"。这样无论模块是高有效还是低有效都适用。
 *
 *  由于 AO 到底接在 IO32 还是 IO33 尚不确定，本版同时监测两路，
 *  取"偏离基线更大"的那一路作为火焰信号来源。
 *
 *  上行 evt：{"ver":"1.0","type":"evt","seq":N,"src":"6","dst":0,
 *             "body":{"device":"flame","event":"detected","level":3}}
 *  上行 dat：{"ver":"1.0","type":"dat","seq":N,"src":"6","dst":0,
 *             "body":{"device":"flame","samples":[{"key":"intensity","value":3120}]}}
 *  上行 sys：online / offline
 * ===================================================================== */

#define BOARD_ID      6
#define BOARD_ID_STR  "6"

const char *SSID        = "wusuijie";
const char *PASSWORD    = "11111111";
const char *MQTT_SERVER = "yunyismart.tech";
const int   MQTT_PORT   = 1883;
const char *MQTT_USER   = "shiyuming";
const char *MQTT_PASS   = "";

const char *TOPIC_CMD    = "202609_PP2/1/cmd";
const char *TOPIC_REPORT = "202609_PP2/1/report";
const char *TOPIC_ONLINE = "202609_PP2/1/online";

// ---- 两路候选 AO 引脚（同时监测）----
const int PIN_AO_1 = 33;    // 候选 1
const int PIN_AO_2 = 32;    // 候选 2

// ---- 判定参数 ----
const int AO_THRESHOLD = 3000;   // AO 偏离基线超过它 → 有火焰（可调）

const unsigned long FLAME_DEBOUNCE_MS = 50;
const unsigned long FLAME_LIMIT_MS    = 500;
const unsigned long FLAME_RESEND_MS   = 5000;
const unsigned long DAT_PERIOD_MS     = 2000;   // 强度上报周期(ms)

WiFiClient   espClient;
PubSubClient client(espClient);
uint32_t seq = 0;

int  base1 = 0, base2 = 0;      // 两路 AO 的开机基线
bool flameRawLast = false;
unsigned long flameRawSince = 0;
bool flameState = false;
unsigned long lastEvtMs = 0;
unsigned long lastDetectedEvtMs = 0;
unsigned long lastDatMs = 0;

uint32_t nextSeq() {
  seq++;
  if (seq == 0) seq = 1;
  return seq;
}

void sendEvt(const char *event, int level) {
  StaticJsonDocument<384> doc;
  doc["ver"]  = "1.0";
  doc["type"] = "evt";
  doc["seq"]  = nextSeq();
  doc["src"]  = BOARD_ID_STR;
  doc["dst"]  = 0;
  JsonObject b = doc.createNestedObject("body");
  b["device"] = "flame";
  b["event"]  = event;
  b["level"]  = level;
  char buf[512];
  serializeJson(doc, buf);
  client.publish(TOPIC_REPORT, buf);
  Serial.print("上报evt: "); Serial.println(buf);
}

void sendDat(int intensity) {
  StaticJsonDocument<384> doc;
  doc["ver"]  = "1.0";
  doc["type"] = "dat";
  doc["seq"]  = nextSeq();
  doc["src"]  = BOARD_ID_STR;
  doc["dst"]  = 0;
  JsonObject b = doc.createNestedObject("body");
  b["device"] = "flame";
  JsonArray s = b.createNestedArray("samples");
  JsonObject o = s.createNestedObject();
  o["key"]   = "intensity";
  o["value"] = intensity;
  char buf[512];
  serializeJson(doc, buf);
  client.publish(TOPIC_REPORT, buf);
  Serial.print("上报dat: "); Serial.println(buf);
}

String offlineWill() {
  StaticJsonDocument<256> doc;
  doc["ver"]  = "1.0";
  doc["type"] = "sys";
  doc["seq"]  = nextSeq();
  doc["src"]  = BOARD_ID_STR;
  doc["dst"]  = 0;
  JsonObject b = doc.createNestedObject("body");
  b["device"] = "sys";
  b["event"]  = "offline";
  char buf[256];
  serializeJson(doc, buf);
  return String(buf);
}

void sendOnline() {
  StaticJsonDocument<256> doc;
  doc["ver"]  = "1.0";
  doc["type"] = "sys";
  doc["seq"]  = nextSeq();
  doc["src"]  = BOARD_ID_STR;
  doc["dst"]  = 0;
  JsonObject b = doc.createNestedObject("body");
  b["device"] = "sys";
  b["event"]  = "online";
  b["uptime"] = (uint32_t)(millis() / 1000);
  char buf[256];
  serializeJson(doc, buf);
  client.publish(TOPIC_ONLINE, buf, true);
  Serial.print("上线: "); Serial.println(buf);
}

// 开机基线校准：采样 2 秒取平均（此时请勿点火）
void calibrateAo() {
  Serial.println("AO基线校准中(请勿点火), 2秒...");
  long s1 = 0, s2 = 0;
  int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 2000) {
    s1 += analogRead(PIN_AO_1);
    s2 += analogRead(PIN_AO_2);
    n++;
    delay(20);
  }
  base1 = s1 / n;
  base2 = s2 / n;
  Serial.print("  AO基线  33="); Serial.print(base1);
  Serial.print("   32="); Serial.print(base2);
  Serial.print("   阈值="); Serial.println(AO_THRESHOLD);
}

// 取"偏离基线更大"的那一路的原始值
int activeAo() {
  int a1 = analogRead(PIN_AO_1);
  int a2 = analogRead(PIN_AO_2);
  return (abs(a1 - base1) >= abs(a2 - base2)) ? a1 : a2;
}

// 打印火焰相关调试信息（数值 + 文字提示）
void printFlameDebug(const char *tag) {
  int a1 = analogRead(PIN_AO_1);
  int a2 = analogRead(PIN_AO_2);
  int d1 = abs(a1 - base1);
  int d2 = abs(a2 - base2);
  bool use1 = (d1 >= d2);
  int diff = use1 ? d1 : d2;
  Serial.println();
  Serial.print("********** "); Serial.print(tag); Serial.println(" **********");
  Serial.print("  IO33: AO="); Serial.print(a1);
  Serial.print("  基线="); Serial.print(base1);
  Serial.print("  偏差="); Serial.println(d1);
  Serial.print("  IO32: AO="); Serial.print(a2);
  Serial.print("  基线="); Serial.print(base2);
  Serial.print("  偏差="); Serial.println(d2);
  Serial.print("  触发引脚="); Serial.print(use1 ? "IO33" : "IO32");
  Serial.print("  偏差="); Serial.print(diff);
  Serial.print("  阈值="); Serial.print(AO_THRESHOLD);
  Serial.print("  判定=");
  Serial.println(diff > AO_THRESHOLD ? "有火焰" : "无火焰");
  Serial.print("  运行时间="); Serial.print(millis() / 1000); Serial.println(" 秒");
  Serial.println("****************************************");
}

// 火焰检测：去抖 50ms / 限流 500ms / 持续检测每 5 秒重发
void scanFlame() {
  int a1 = analogRead(PIN_AO_1);
  int a2 = analogRead(PIN_AO_2);
  int d1 = abs(a1 - base1);
  int d2 = abs(a2 - base2);
  int diff = (d1 >= d2) ? d1 : d2;    // 两路里偏离更大的
  bool raw = (diff > AO_THRESHOLD);   // 超过阈值 = 有火焰

  if (raw != flameRawLast) {
    flameRawLast = raw;
    flameRawSince = millis();
    return;
  }
  if (millis() - flameRawSince < FLAME_DEBOUNCE_MS) return;

  if (raw != flameState) {
    flameState = raw;
    if (millis() - lastEvtMs >= FLAME_LIMIT_MS) {
      if (flameState) {
        Serial.println("[火焰] >>>>> 检测到火焰! <<<<<");
        printFlameDebug("检测到火焰");
        sendEvt("detected", 3);
        lastDetectedEvtMs = millis();
      } else {
        Serial.println("[火焰] >>>>> 火焰消失 <<<<<");
        printFlameDebug("火焰消失");
        sendEvt("clear", 0);
      }
      lastEvtMs = millis();
    }
  } else if (flameState) {
    if (millis() - lastDetectedEvtMs >= FLAME_RESEND_MS) {
      Serial.println("[火焰] 持续检测中，重发一次...");
      printFlameDebug("持续检测中");
      sendEvt("detected", 3);
      lastDetectedEvtMs = millis();
    }
  }
}

// 本板为传感器节点，无下行控制命令；收到 cmd 仅打印不处理
void callback(char *topic, byte *payload, unsigned int len) {
  char buf[256];
  unsigned int n = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  Serial.print("收到: "); Serial.println(buf);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("连接wifi中(");
  Serial.print(SSID);
  Serial.print(")");
  int t = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    t++;
    if (t % 10 == 0) { Serial.print(" status="); Serial.print((int)WiFi.status()); }
    if (t >= 60) {
      Serial.println();
      Serial.println("wifi连接失败, 扫描周边网络:");
      int n = WiFi.scanNetworks();
      Serial.print("  共"); Serial.print(n); Serial.println("个:");
      for (int i = 0; i < n && i < 20; i++) {
        Serial.print("   ");
        Serial.print(WiFi.SSID(i));
        Serial.print("  CH");
        Serial.print(WiFi.channel(i));
        Serial.print("  enc=");
        Serial.println((int)WiFi.encryptionType(i));
      }
      Serial.println("重启重试...");
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("wifi连接成功, ip = ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("连接服务器中...");
    String clientId = "esp32-" + String(BOARD_ID) + "-" + WiFi.macAddress();
    String will = offlineWill();
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, TOPIC_ONLINE, 1, true, will.c_str())) {
      Serial.println("服务器连接成功");
      client.subscribe(TOPIC_CMD, 1);
      Serial.print("已订阅: "); Serial.println(TOPIC_CMD);
      sendOnline();
    } else {
      Serial.print("连接失败, err="); Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== 6号板 火焰传感器模块(纯AO版) ===");

  analogReadResolution(12);   // 12 位 ADC，0~4095

  calibrateAo();              // 开机基线校准（此时请勿点火）

  connectWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(512);
  reconnectMQTT();
  Serial.println("系统就绪，开始采集...");
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  scanFlame();

  // 周期性上报火焰强度，并打印两路 AO 便于观察
  if (millis() - lastDatMs >= DAT_PERIOD_MS) {
    lastDatMs = millis();
    int a1 = analogRead(PIN_AO_1);
    int a2 = analogRead(PIN_AO_2);
    int d1 = abs(a1 - base1);
    int d2 = abs(a2 - base2);
    Serial.print("[监测] AO33="); Serial.print(a1);
    Serial.print("(偏"); Serial.print(d1);
    Serial.print(")  AO32="); Serial.print(a2);
    Serial.print("(偏"); Serial.print(d2);
    Serial.print(")  阈值="); Serial.println(AO_THRESHOLD);
    sendDat(activeAo());
  }
}
