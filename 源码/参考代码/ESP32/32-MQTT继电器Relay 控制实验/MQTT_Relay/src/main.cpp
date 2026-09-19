#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char *ssid = "IOT-SZCS02";             // ESP32连接的WiFi账号
const char *password = "iot-szcs02";         // WiFi密码
const char *mqttServer = "yunyismart.tech";  // 要连接到的服务器IP
const int mqttPort = 1883;                   // 要连接到的服务器端口号
const char *mqttUser = "";                   // MQTT服务器账号
const char *mqttPassword = "";               // MQTT服务器密码

WiFiClient espClient;            // 定义wifiClient实例
PubSubClient client(espClient);  // 定义PubSubClient的实例

String Sub_topic = "/szcu/202609PP2/set";
String Pub_topic = "/szcu/202609PP2/post";

String message = "";
// 上报消息的数据格式
#define JSON_RELAY_ON  "{\"RelayState\":\"on\"}"
#define JSON_RELAY_OFF "{\"RelayState\":\"off\"}"

#define PIN_RELAY      14  // 定义继电器控制引脚GPIO14

void postMsg(String commType, String sensorType, String msg)
{
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

void parseMqttResponse(char *payload)
{
    Serial.println("start parse Mqtt Response...");
    DynamicJsonDocument jsonBuffer(100);
    DeserializationError error = deserializeJson(jsonBuffer, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    else
    {
        JsonObject root = jsonBuffer.as<JsonObject>();
        String commType = root["commType"];
        String sensorType = root["sensorType"];
        String cmd = root["cmd"];

        if (commType == "Wifi")
        {
            // 继电器
            if (sensorType == "Relay")
            {
                if (root["cmd"]["RelayCtr"] == 1)
                {
                    digitalWrite(PIN_RELAY, HIGH);
                    postMsg("Wifi", "Relay", JSON_RELAY_ON);
                }
                else if (root["cmd"]["RelayCtr"] == 0)
                {
                    digitalWrite(PIN_RELAY, LOW);
                    postMsg("Wifi", "Relay", JSON_RELAY_OFF);
                }
            }
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    // 打印收到的数据包
    Serial.print("Topic:[");
    Serial.print(topic);
    Serial.println("] ");
    Serial.println(length);
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // 消息处理
    parseMqttResponse((char *)payload);
}

void setup()
{
    // 串口函数，波特率设置
    Serial.begin(115200);
    Serial.println("MQTT 继电器控制报实验...");

    pinMode(PIN_RELAY, OUTPUT);    // 初始化PIN_RELAY引脚模式为输出
    digitalWrite(PIN_RELAY, LOW);  // 初始Relay为低电平，表示Relay 关

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("连接wifi中...");
        delay(1000);
    }
    Serial.println("wifi连接成功");

    // MQTT服务器连接函数（服务器IP，端口号）
    client.setServer(mqttServer, mqttPort);
    // 设定回调方式，当ESP32收到订阅消息时会调用此方法
    client.setCallback(callback);
    // 是否连接上MQTT服务器
    while (!client.connected())
    {
        Serial.println("连接服务器中");
        String client_id = "esp32-client-" + String(WiFi.macAddress());
        if (client.connect(client_id.c_str(), mqttUser, mqttPassword))
        {
            Serial.println("服务器连接成功");
        }
        else
        {
            Serial.print("连接服务器失败");
            Serial.print(client.state());
            delay(2000);
        }
    }

    // 连接MQTT服务器后订阅主题
    client.subscribe(Sub_topic.c_str());
    // 串口打印：已订阅主题，等待主题消息
    Serial.print("已订阅主题，等待主题消息....");
}

void loop()
{
    if (!client.connected())
    {
        Serial.println("client 连接服务器失败...");
    }
    client.loop();
}