#include <Arduino.h>
#include <ESP32Servo.h>
// 安装ServoESP32库
// 接线 棕色 负极   红色 5V    橘黄色 信号线 14
// 控制舵机正转90度  反转90度回至0度

static const int servoPin = 14;
Servo servo;

void setup()
{
    servo.attach(servoPin);
}

void loop()
{
    servo.write(0);
    delay(2000);
    servo.write(90);
    delay(2000);
}