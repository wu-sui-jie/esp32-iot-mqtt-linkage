#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define PIN_LED1 2   // 定义LED1
#define PIN_KEY2 39  // 定义KEY2

const char *ssid = "IOT-SZCS02";             // ESP32连接的WiFi账号
const char *password = "iot-szcs02";         // WiFi密码
const char *mqttServer = "yunyismart.tech";  // 要连接到的服务器IP
const int mqttPort = 1883;                   // 要连接到的服务器端口号
const char *mqttUser = "";                   // MQTT服务器账号
const char *mqttPassword = "";               // MQTT服务器密码
const char *topic = "szcu20260914";          // 定义主题
String message = "";

WiFiClient espClient;            // 定义wifiClient实例
PubSubClient client(espClient);  // 定义PubSubClient的实例

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("来自订阅的主题:");  // 串口打印：来自订阅的主题:
    Serial.println(topic);            // 串口打印订阅的主题
    Serial.print("信息：");           // 串口打印：信息：
    for (int i = 0; i < length; i++)  // 使用循环打印接收到的信息
    {
        Serial.print((char)payload[i]);
        message += (char)payload[i];
    }

    if (message == "led1:on")
    {
        digitalWrite(PIN_LED1, LOW);
    }
    else if (message == "led1:off")
    {
        digitalWrite(PIN_LED1, HIGH);
    }
    Serial.println();
    Serial.println("-----------------------");
    message = "";
}

// 生成基于ESP32唯一ID的MQTT ClientID
String generateMQTTClientID()
{
    // 方式1：使用ESP32芯片唯一ID（64位，推荐）
    uint64_t chipId = ESP.getEfuseMac();   // 获取芯片MAC地址（唯一）
    uint32_t chipId32 = (uint32_t)chipId;  // 转32位简化

    // 拼接ClientID（格式：ESP32_ + 芯片ID后8位十六进制 + 随机数（可选））
    String clientId = "SZCU_";
    clientId += String(chipId32, HEX);  // 芯片ID转十六进制字符串
    clientId += "_";
    clientId += random(1000, 9999);  // 加4位随机数，进一步避免冲突（可选）

    // 可选：转大写，确保格式统一
    clientId.toUpperCase();

    return clientId;
}

void setup()
{
    Serial.begin(115200);  // 串口函数，波特率设置

    pinMode(PIN_LED1, OUTPUT);     // 初始化PIN_LED引脚模式为输出
    digitalWrite(PIN_LED1, HIGH);  // 初始LED灯置为高电平，表示LED熄灭

    pinMode(PIN_KEY2, INPUT);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("连接wifi中");
        delay(1000);
    }
    Serial.println("wifi连接成功");
    Serial.print("ip = ");
    Serial.println(WiFi.localIP());

    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    String clientID = generateMQTTClientID();
    Serial.printf("ClientID:");
    Serial.println(clientID);

    while (!client.connected())
    {
        Serial.println("连接服务器中");
        if (client.connect(clientID.c_str(), mqttUser, mqttPassword))
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
    client.subscribe(topic);
    Serial.print("已订阅主题，等待主题消息....");
}

void loop()
{
    static bool key2_pressed = false;

    // 回旋接收函数  等待服务器返回的数据
    client.loop();

    // 如果按键状态有变化，则发布按键的消息
    if (key2_pressed)
    {
        if (digitalRead(PIN_KEY2))
        {
            key2_pressed = false;
            Serial.printf("key2:%d\r\n", key2_pressed);

            // 发布MQTT消息
            client.publish(topic, "key2_pressed:false");
        }
    }
    else
    {
        if (!digitalRead(PIN_KEY2))
        {
            key2_pressed = true;
            Serial.printf("key2:%d\r\n", key2_pressed);

            client.publish(topic, "key2_pressed:true");
        }
    }
    delay(50);
}