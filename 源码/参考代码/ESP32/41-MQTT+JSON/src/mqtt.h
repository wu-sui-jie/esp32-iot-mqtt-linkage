#pragma once

#include <Arduino.h>

// MQTT连接
void mqtt_connect();

// MQTT服务器是否已经连接
bool mqtt_connected();

// MQTT循环
void mqtt_loop();

// 当MQTT收到消息时，会执行该函数
void mqtt_callback(char *topic, byte *payload, unsigned int length);

// MQTT发布消息
void mqtt_post(String device, String key, String value);