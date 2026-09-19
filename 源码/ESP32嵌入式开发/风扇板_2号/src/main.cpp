#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* =====================================================================
 *  2 号板 —— 继电器（风扇）模块
 *  严格遵循《MQTT 通信协议规范 V2.0（第 1 组）》
 *  仅实现风扇控制（不含 LED / KEY）
 *
 *  下行 cmd：{"ver":"1.0","type":"cmd","seq":2,"src":"web","dst":2,
 *             "body":{"device":"fan","action":"set_power","param":{"on":true}}}
 *  上行 ack：{"ver":"1.0","type":"ack","seq":N,"src":"2","dst":0,
 *             "body":{"device":"fan","action":"set_power","result":"ok","param":{"on":true}}}
 *  上行 sys：online / offline（online 主题，Retain）
 * ===================================================================== */

#define BOARD_ID      2
#define BOARD_ID_STR  "2"

const char *SSID        = "wusuijie";
const char *PASSWORD    = "11111111";
const char *MQTT_SERVER = "yunyismart.tech";
const int   MQTT_PORT   = 1883;
const char *MQTT_USER   = "shiyuming";
const char *MQTT_PASS   = "";

const char *TOPIC_CMD    = "202609_PP2/1/cmd";
const char *TOPIC_REPORT = "202609_PP2/1/report";
const char *TOPIC_ONLINE = "202609_PP2/1/online";

const int PIN_FAN  = 14;   // 风扇（PWM）
const int FAN_CH   = 0;
const int FAN_FREQ = 5000;
const int FAN_RES  = 8;

WiFiClient   espClient;
PubSubClient client(espClient);
uint32_t seq = 0;

uint32_t nextSeq() {
  seq++;
  if (seq == 0) seq = 1;
  return seq;
}

void sendAck(int seqNum, bool on) {
  StaticJsonDocument<384> doc;
  doc["ver"]  = "1.0";
  doc["type"] = "ack";
  doc["seq"]  = seqNum;
  doc["src"]  = BOARD_ID_STR;
  doc["dst"]  = 0;
  JsonObject b = doc.createNestedObject("body");
  b["device"] = "fan";
  b["action"] = "set_power";
  b["result"] = "ok";
  JsonObject p = b.createNestedObject("param");
  p["on"] = on;
  char buf[512];
  serializeJson(doc, buf);
  client.publish(TOPIC_REPORT, buf);
  Serial.print("上报ack: "); Serial.println(buf);
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

void fanSet(bool on) {
  ledcWrite(FAN_CH, on ? 255 : 0);   // on = 全速，off = 停
  Serial.print("[风扇] ");
  Serial.println(on ? "开" : "关");
}

void callback(char *topic, byte *payload, unsigned int len) {
  char buf[512];
  unsigned int n = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  Serial.print("收到: "); Serial.println(buf);

  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, buf)) {
    Serial.println("deserializeJson() failed");
    return;
  }
  const char *ver  = doc["ver"];
  const char *type = doc["type"];
  int dst = doc["dst"];
  if (strcmp(ver, "1.0") != 0) { Serial.println("版本不符,忽略"); return; }
  if (strcmp(type, "cmd") != 0) return;
  if (dst != 0 && dst != BOARD_ID) return;

  JsonObject b       = doc["body"];
  const char *device = b["device"];
  const char *action = b["action"];
  int seqNum = doc["seq"];

  if (strcmp(device, "fan") == 0 && strcmp(action, "set_power") == 0) {
    bool on = b["param"]["on"];
    fanSet(on);
    sendAck(seqNum, on);
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);       // 不把WiFi写入flash，避免旧配置干扰
  WiFi.disconnect(true, true);  // 清除之前保存的WiFi信息
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
    if (t >= 60) {   // 约30秒仍连不上
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
  Serial.println("=== 2号板 风扇模块 ===");

  ledcSetup(FAN_CH, FAN_FREQ, FAN_RES);
  ledcAttachPin(PIN_FAN, FAN_CH);
  ledcWrite(FAN_CH, 0);

  connectWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(512);
  reconnectMQTT();
  Serial.println("系统就绪，等待命令...");
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();
}
