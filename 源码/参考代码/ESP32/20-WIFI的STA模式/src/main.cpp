#include <Arduino.h>
#include <WiFi.h>

const char *ssid = "IOT-SZCS02";
const char *password = "iot-szcs02";

// 生成基于ESP32唯一ID的序列号
String get_SN()
{
    // 使用ESP32芯片唯一ID（64位，推荐）
    uint64_t chipId = ESP.getEfuseMac();  // 获取芯片MAC地址（唯一）
    uint32_t chipId32 = (uint32_t)chipId; // 转32位简化

    // 拼接 SN（格式：前缀 + 芯片ID + 随机数）
    String SN = "SZCU_";
    SN += String(chipId32, HEX); // 芯片ID转十六进制字符串
    SN += "_";
    SN += random(1000, 9999); // 加4位随机数，进一步避免冲突（可选）

    // 可选：转大写，确保格式统一
    SN.toUpperCase();

    return SN;
}

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.disconnect();
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    delay(1000);
    Serial.print("+");
}
