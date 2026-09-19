#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// 定义LED、KEY
const int PIN_LED1 = 2;
const int PIN_KEY2 = 39;

void LED1_on() { digitalWrite(PIN_LED1, LOW); }
void LED1_off() { digitalWrite(PIN_LED1, HIGH); }
bool KEY2_pressed() { return digitalRead(PIN_KEY2) ? false : true; }

const char *ssid = "IOT-SZCS02";            // ESP32连接的WiFi账号
const char *password = "iot-szcs02";        // WiFi密码
const char *mqttServer = "yunyismart.tech"; // 要连接到的服务器IP
const int mqttPort = 1883;                  // 要连接到的服务器端口号
const char *mqttUser = "xx";                // MQTT服务器账号
const char *mqttPassword = "xx";            // MQTT服务器密码

String message = "";
WiFiClient espClient;           // 定义wifiClient实例
PubSubClient client(espClient); // 定义PubSubClient的实例

void callback(char *topic, byte *payload, unsigned int length)
{
    message = "";
    Serial.print("来自订阅的主题:"); // 串口打印：来自订阅的主题:
    Serial.println(topic);           // 串口打印订阅的主题
    Serial.print("信息：");          // 串口打印：信息：
    for (int i = 0; i < length; i++) // 使用循环打印接收到的信息
    {
        Serial.print((char)payload[i]);
        message += (char)payload[i];
    }

    if (message == "led:on")
    {
        LED1_on();
    }
    else if (message == "led:off")
    {
        LED1_off();
    }
    Serial.println();
    Serial.println("-----------------------");
}

// 生成基于ESP32唯一ID的MQTT ClientID
String generateMQTTClientID()
{
    // 使用ESP32芯片唯一ID（64位，推荐）
    uint64_t chipId = ESP.getEfuseMac();  // 获取芯片MAC地址（唯一）
    uint32_t chipId32 = (uint32_t)chipId; // 转32位简化

    // 拼接ClientID（格式：ESP32_ + 芯片ID后8位十六进制 + 随机数（可选））
    String clientId = "SZCU_";
    clientId += String(chipId32, HEX); // 芯片ID转十六进制字符串
    clientId += "_";
    clientId += random(1000, 9999); // 加4位随机数，进一步避免冲突（可选）

    // 可选：转大写，确保格式统一
    clientId.toUpperCase();

    return clientId;
}

void setup()
{
    // 串口初始化
    Serial.begin(115200);

    // LED1初始化
    pinMode(PIN_LED1, OUTPUT); // 引脚模式为输出
    LED1_off();
    // KEY2初始化
    pinMode(PIN_KEY2, INPUT);

    // 连接Wifi
    WiFi.begin(ssid, password);           // 接入WiFi函数（WiFi名称，密码）重新连接wif
    while (WiFi.status() != WL_CONNECTED) // 若WiFi接入成功WiFi.status()会返回 WL_CONNECTED
    {
        Serial.println("连接wifi中"); // 串口输出：连接wifi中
        delay(1000);                  // 若尚未连接WiFi，则进行重连WiFi的循环
    }
    Serial.println("wifi连接成功"); // 连接wifi成功之后会跳出循环，串口并输出：wifi连接成功
    Serial.print("ip = ");
    Serial.println(WiFi.localIP());

    // 连接MQTT服务器
    client.setServer(mqttServer, mqttPort); // MQTT服务器连接函数（服务器IP，端口号）
    client.setCallback(callback);           // 设定回调方式，当ESP32收到订阅消息时会调用此方法
    String clientID = generateMQTTClientID();
    Serial.printf("ClientID:");
    Serial.println(clientID);

    while (!client.connected()) // 是否连接上MQTT服务器
    {
        Serial.println("连接服务器中");                               // 串口打印：连接服务器中
        if (client.connect(clientID.c_str(), mqttUser, mqttPassword)) // 如果服务器连接成功
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

    // 订阅消息
    client.subscribe("pub_ESP32");                // 连接MQTT服务器后订阅主题
    Serial.print("已订阅主题，等待主题消息...."); // 串口打印：已订阅主题，等待主题消息
}

// 按键2上次是否按下，用于跟踪按键状态
bool key2_pressed_last = false;

void loop()
{
    // 网络消息处理
    client.loop();

    // 如果按键之前是被按下的
    if (key2_pressed_last)
    {
        // 现在用户松开按键
        if (!KEY2_pressed())
        {
            // 更新按键状态上次值
            key2_pressed_last = false;
            // 使用串口发送消息
            Serial.printf("key2 pressed\r\n");
            // 发布MQTT消息
            client.publish("sub_ESP32", "key2 pressed");
        }
    }
    // 否则之前是弹起的
    else
    {
        // 现在用户按下按键
        if (KEY2_pressed())
        {
            // 更新按键状态上次值
            key2_pressed_last = true;
            // 使用串口发送消息
            Serial.printf("key2 released\r\n");
            // 发布MQTT消息
            client.publish("sub_ESP32", "key2 released");
        }
    }

    // 一定的延迟，也可以删掉
    delay(50);
}