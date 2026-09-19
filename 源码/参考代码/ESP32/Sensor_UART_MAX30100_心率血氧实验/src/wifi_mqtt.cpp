#include "wifi_mqtt.h"
#include <WiFi.h>

extern String wifi_ssid;  //wifi账号密码
extern String wifi_pass;  //wifi账号密码
extern String mqttip;     //mqttip
extern int mqttstate;

String fpmCMD = "";

String myDeviceID = "";

String Sub_topic = "";
String Pub_topic = "";

const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
long now = 0;
char tmp_msg[20];
char msg[100];
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println("Connecting to ");
  Serial.println(wifi_ssid);
  Serial.println(wifi_pass);

  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  // WiFi.begin(ssid2, password2);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


bool clientIsConnected() {
  return client.connected();
}

void clientLoop() {
  client.loop();
}

void clientInit() {
  Serial.println(mqttip);
  client.setServer(mqttip.c_str(), mqtt_port);
  client.setCallback(callback);
  mqttstate = client.connected() ? 1 : 0;
}

void SSID_init() {
  Serial.println("SSID init...");
  Serial.println(wifi_ssid);
  Serial.println(wifi_pass);
  Serial.println(mqttip);
}

void Topic_init(String id) {
  myDeviceID = id;
  Sub_topic = "/IOT/" + myDeviceID + "/set";
  Pub_topic = "/IOT/" + myDeviceID + "/post";
  Serial.println("TOPIC init...");
  Serial.println(myDeviceID);
  Serial.println(Sub_topic);
  Serial.println(Pub_topic);
}

/**
   断开重连
*/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("尝试连接MQTT服务器中...");
    // Create a random client ID
    String client_id = "home" + WiFi.macAddress();
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());

    // Attempt to connect
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public emqx mqtt broker connected");
      Serial.println(client.state());
      digitalWrite(2, HIGH);
      mqttstate=1;
    } else {
      Serial.print("failed with state :");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      mqttstate=0;
      // Wait 2 seconds before retrying
      digitalWrite(2, LOW);
      delay(2000);
    }
  }
  client.subscribe(Sub_topic.c_str());
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Topic:[");
  Serial.print(topic);
  Serial.println("] ");
  Serial.println(length);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //消息处理
  parseMqttResponse((char *)payload);
}

String tmpstr = "";
int err_count = 0;
void CommandtoNBIOT(String cmd, char *res) {
  while (1) {
    Serial2.print(cmd);  //不要有换行
    Serial2.flush();
    delay(300);
    while (Serial2.available() > 0) {
      if (Serial2.find(res)) {
        Serial.print(cmd);
        Serial.println("  OK  ");
        return;
      } else {
        Serial.print(cmd);
        Serial.println("  ERROR");
        err_count++;
        if (err_count > 5)  //错误超过5次 退出本次发送
          break;
      }
    }
    delay(500);
  }
}

/**
   解析mqtt数据
   消息格式
   {"commType":"Wifi","sensorType":"BEEP","cmd":{"BEEP_Com":"0"}}
*/
void parseMqttResponse(char *payload) {

  Serial.println("start parse Mqtt Response...");

  DynamicJsonDocument jsonBuffer(100);
  DeserializationError error = deserializeJson(jsonBuffer, payload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  } else {
    JsonObject root = jsonBuffer.as<JsonObject>();
    String commType = root["commType"];
    Serial.println(commType);
    String sensorType = root["sensorType"];
    Serial.println(sensorType);
    String cmd = root["cmd"];
    Serial.println(cmd);

    //判断commType
    // // 如果是Zigbee
    // if (commType == "Zigbee") {
    //   char jsonBuffer[100];
    //   root["commType"] = "ZigCoor";
    //   serializeJson(root, jsonBuffer);
    //   Serial.println(String(jsonBuffer));
    //   CommandtoNBIOT(String(jsonBuffer), "OK");
    // }
    //如果是WiFi
    /*
    if (commType == "Wifi") {
      //声光报警器控制
      if (sensorType == "Alarm") {
        cmdflag.Alarm = root["cmd"]["Alarm"];
        if (cmdflag.Alarm == 1) {
          Serial.println("Alarm开");
        } else if (cmdflag.Alarm == 0) {
          Serial.println("Alarm关");
        }
      }
      //门锁控制控制
      if (sensorType == "Door") {
        if (root["cmd"]["Lock"] == 1) {
          cmdflag.DoorLock = 1;
        } else if (root["cmd"]["Lock"] == 0) {
          cmdflag.DoorLock = 0;
        }
      }
      //无线继电器
      if (sensorType == "Relay") {
        if (root["cmd"]["Relay"] == 1) {
          cmdflag.Relay = 1;
        } else if (root["cmd"]["Relay"] == 0) {
          cmdflag.Relay = 0;
        }
      }
      //风扇控制
      if (sensorType == "Fan") {
        if (root["cmd"]["Fan"] == 1) {
          cmdflag.Fan = 1;
        } else if (root["cmd"]["Fan"] == 0) {
          cmdflag.Fan = 0;
        }
      }
      //舵机控制
      if (sensorType == "MG90S") {
        cmdflag.MG90S = root["cmd"]["MG90S"];
      }
      //指纹模块控制
      if (sensorType == "Fpm") {
        if (cmd == "total") {
          cmdflag.FpmCMD = 5;
        } else if (cmd == "empty") {
          cmdflag.FpmCMD = 3;
        } else if (cmd == "delete") {
          cmdflag.FpmCMD = 4;
          cmdflag.FpmID = root["id"];
        } else if (cmd == "enroll") {
          cmdflag.FpmCMD = 2;
          cmdflag.FpmID = root["id"];
        } else if (cmd == "identify") {
          cmdflag.FpmCMD = 1;
        } else if (cmd == "DoorLock") {
          cmdflag.FpmCMD = 10;
          cmdflag.FpmID = root["id"];
        } else if (cmd == "sleep") {
          cmdflag.FpmCMD = 6;
        }
      }
    }*/
  }
}

void postMsg(String commType, String sensorType, String msg) {
  StaticJsonDocument<200> doc;
  doc["commType"] = commType;
  doc["sensorType"] = sensorType;
  doc["data"] = msg;

  char JSONmsgBuffer[100];
  serializeJson(doc, JSONmsgBuffer);

  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmsgBuffer);

  client.publish(Pub_topic.c_str(), JSONmsgBuffer);
}

void postStr(String str) {
 // Serial.println(str);
  client.publish(Pub_topic.c_str(), (char *)str.c_str());
}
