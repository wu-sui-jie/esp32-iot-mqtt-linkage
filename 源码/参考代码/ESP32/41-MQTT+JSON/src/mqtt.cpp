
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "my_config.h"
#include "mqtt.h"
#include "main.h"

// 定义wifiClient实例
WiFiClient espClient;

// 定义PubSubClient的实例
PubSubClient client(espClient);

// MQTT连接
void mqtt_connect()
{
    // 连接WiFi
    WiFi.begin(SSID, PASSWORD);           // 接入WiFi函数（WiFi名称，密码）
    while (WiFi.status() != WL_CONNECTED) // 若WiFi接入成功WiFi.status()会返回 WL_CONNECTED
    {
        Serial.println("连接wifi中..."); // 串口输出：连接wifi中
        delay(1000);                     // 若尚未连接WiFi，则进行重连WiFi的循环
    }
    Serial.println("wifi连接成功"); // 连接wifi成功之后会跳出循环，串口并输出：wifi连接成功

    // 连接MQTT服务器
    client.setServer(mqttServer, mqttPort); // 连接MQTT服务器（服务器IP，端口号）
    client.setCallback(mqtt_callback);      // 设定回调方式，当ESP32收到订阅消息时会调用此方法
    while (!client.connected())             // 是否连接上MQTT服务器
    {
        Serial.println("连接服务器中"); // 串口打印：连接服务器中
        String client_id = "esp32-client-" + String(WiFi.macAddress());
        if (client.connect(client_id.c_str(), "", "")) // 如果服务器连接成功
        {
            Serial.println("服务器连接成功"); // 串口打印：服务器连接成功
        }
        else
        {
            Serial.print("连接服务器失败"); // 串口打印：连接服务器失败
            Serial.print(client.state());   // 重新连接函数
            delay(2000);
        }
    }

    // 订阅主题
    client.subscribe(TOPIC);
    Serial.printf("已订阅主题 %s，等待主题消息....\r\n", TOPIC); // 串口打印：已订阅主题，等待主题消息
}

// MQTT服务器是否已经连接
bool mqtt_connected()
{
    return client.connected();
}

// MQTT循环
void mqtt_loop()
{
    client.loop();
}

// 当MQTT收到消息时，会执行该函数
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("callback");

    // 先缓存收到的消息
    char msg[256];
    length = length <= 255 ? length : 255;
    memcpy(msg, payload, length);
    msg[length] = 0;

    // 调试
    Serial.printf("get topic:%s, message:%s\r\n", topic, msg);

    // 解析JSON格式
    DynamicJsonDocument jsonBuffer(256);
    DeserializationError error = deserializeJson(jsonBuffer, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // 解析JSON内容
    JsonObject root = jsonBuffer.as<JsonObject>();
    String device = root["device"];
    String key = root["key"];
    String value = root["value"];

    handle_device_controls(device, key, value);
}

// MQTT发布消息
void mqtt_post(String device, String key, String value)
{
    // 构造JSON对象
    StaticJsonDocument<200> doc;
    doc["device"] = device;
    doc["key"] = key;
    doc["value"] = value;

    // 将JSON对象转换为字符串
    char json_msg[256];
    serializeJson(doc, json_msg);

    // 发出消息
    client.publish(TOPIC, json_msg);

    // 调试
    Serial.printf("send to topic:%s, message:%s\r\n", TOPIC, json_msg);
}
