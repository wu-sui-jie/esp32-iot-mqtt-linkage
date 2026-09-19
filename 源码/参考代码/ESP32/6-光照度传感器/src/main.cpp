#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

void setup()
{
    Serial.begin(115200);
    Serial.println(F("BH1750 Test"));

    // 使用lightMeter库来控制光照度传感器
    Wire.begin();
    lightMeter.begin();
}

void loop()
{
    float lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
    delay(1000);
}