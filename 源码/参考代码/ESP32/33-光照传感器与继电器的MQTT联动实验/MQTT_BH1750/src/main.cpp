#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

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

#define BUF_DATA_LEN 150
char ATdata[BUF_DATA_LEN];


void cleanBuffer(char *buf, int len)
{
    for (int i = 0; i < len; i++)
    {
        buf[i] = '\0';
    }
}

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

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("来自订阅的主题:");  // 串口打印：来自订阅的主题:
    Serial.println(topic);            // 串口打印订阅的主题
    Serial.print("信息：");           // 串口打印：信息：
    for (int i = 0; i < length; i++)  // 使用循环打印接收到的信息
    {
        Serial.print((char)payload[i]);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println(F("BH1750 MQTT 数据上报实验..."));

    Wire.begin();
    lightMeter.begin();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("连接wifi中...");
        delay(1000);
    }
    Serial.println("wifi连接成功");

    // MQTT服务器连接函数
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
}

void loop()
{
    static unsigned long lastMsMain = 0;
    if (!client.connected())
    {
        Serial.println("client 连接服务器失败...");
    }
    client.loop();


    if (millis() - lastMsMain >= 2000)
    {
        lastMsMain = millis();

        float lux = lightMeter.readLightLevel();

        cleanBuffer(ATdata, BUF_DATA_LEN);
        int len = snprintf(ATdata, BUF_DATA_LEN, "{\"illuminance\":%.2f}", lux);

        Serial.println(ATdata);
        
        postMsg("Wifi", "BH1750", ATdata);


        // ==========================================================
        // 与 31 号程序相比，增加了以下几句代码
        // ==========================================================

        // 如果光照太暗，则打开继电器；如果光照太亮，则关闭继电器
        if ((lux <= 10) && (relaycmd != 1))
        {
            relaycmd = 1;
            Serial.println("光照度低于10lux,打开继电器");
            postStr(JSON_DATA_RELAY_ON);
        }
        else if ((lux > 10) && (relaycmd != 0))
        {
            relaycmd = 0;
            Serial.println("光照度高于10lux,关闭继电器");
            postStr(JSON_DATA_RELAY_OFF);
        }
    }
}
